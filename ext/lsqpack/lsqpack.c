#include "lsqpack.h"

#include <stdio.h>

#include "ls-qpack/lsqpack.h"

#define HEADER_BUF_SIZE 4096
#define ENCODER_BUF_SIZE 4096
#define DECODER_BUF_SIZE 4096
#define PREFIX_MAX_SIZE 16

VALUE rb_mLsqpack;
VALUE rb_clsEncoder;
VALUE rb_clsDecoder;
VALUE rb_eError;
VALUE rb_eDecompressionFailed;
VALUE rb_eDecoderStreamErr;
VALUE rb_eEncoderStreamErr;
VALUE rb_eStreamBlocked;

struct header_block {
  STAILQ_ENTRY(header_block) entries;
  int blocked : 1;
  unsigned char* data;
  size_t data_len;
  const unsigned char* data_ptr;
  struct lsqpack_header_list* hlist;
  uint64_t stream_id;
};

static struct header_block* header_block_new(size_t stream_id, const unsigned char* data, size_t data_len) {
  struct header_block* hblock = malloc(sizeof(struct header_block));
  memset(hblock, 0, sizeof(*hblock));
  hblock->data = malloc(data_len);
  hblock->data_len = data_len;
  hblock->data_ptr = hblock->data;
  memcpy(hblock->data, data, data_len);
  hblock->stream_id = stream_id;
  return hblock;
}

static void header_block_free(struct header_block* hblock) {
  free(hblock->data);
  hblock->data = 0;
  hblock->data_ptr = 0;
  if (hblock->hlist) {
    lsqpack_dec_destroy_header_list(hblock->hlist);
    hblock->hlist = 0;
  }
  free(hblock);
}

VALUE hlist_to_headers(struct lsqpack_header_list* hlist) {
  VALUE list, tuple, name, value;
  struct lsqpack_header* header;

  list = rb_ary_new_capa(hlist->qhl_count);
  for (size_t i = 0; i < hlist->qhl_count; i++) {
    header = hlist->qhl_headers[i];
    name = rb_str_new(header->qh_name, header->qh_name_len);
    value = rb_str_new(header->qh_value, header->qh_value_len);
    tuple = rb_ary_new_capa(2);
    rb_ary_store(tuple, 0, name);
    rb_ary_store(tuple, 1, value);
    rb_ary_store(list, i, tuple);
  }
  return list;
}

static void header_unblocked(void* opaque) {
  struct header_block* hblock = opaque;
  hblock->blocked = 0;
}

typedef struct {
  struct lsqpack_dec dec;
  unsigned char decoder_buf[DECODER_BUF_SIZE];
  STAILQ_HEAD(, header_block) pending_blocks;
} DecoderObj;

size_t lsqpackrb_dec_size(DecoderObj* data) {
  return sizeof(DecoderObj);  // Is it correct?
}

void lsqpackrb_dec_free(DecoderObj* data) {
  struct header_block* hblock;
  lsqpack_dec_cleanup(&data->dec);
  while (!STAILQ_EMPTY(&data->pending_blocks)) {
    hblock = STAILQ_FIRST(&data->pending_blocks);
    STAILQ_REMOVE_HEAD(&data->pending_blocks, entries);
    header_block_free(hblock);
  }
  free(data);
}

static const rb_data_type_t decoder_data_type = {
    .wrap_struct_name = "lsqpack_dec_rb",
    .function =
        {
            .dmark = NULL,
            .dfree = lsqpackrb_dec_free,
            .dsize = lsqpackrb_dec_size,
        },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE lsqpackrb_dec_alloc(VALUE self) {
  DecoderObj* data = malloc(sizeof(DecoderObj));
  return TypedData_Wrap_Struct(self, &decoder_data_type, data);
}

VALUE lsqpackrb_dec_init(int argc, VALUE* argv, VALUE self) {
  DecoderObj* data;
  TypedData_Get_Struct(self, DecoderObj, &decoder_data_type, data);

  VALUE hash;
  hash = rb_check_hash_type(argv[argc - 1]);
  unsigned int max_tbl_capa, blked_streams;
  ID table[2];
  VALUE values[2];
  table[0] = rb_intern("max_table_capacity");
  table[1] = rb_intern("blocked_streams");
  rb_get_kwargs(hash, table, 0, 2, values);
  max_tbl_capa = (values[0] != Qundef) ? (FIX2INT(values[0])) : 0;
  blked_streams = (values[1] != Qundef) ? (FIX2INT(values[1])) : 0;

  lsqpack_dec_init(&data->dec, NULL, max_tbl_capa, blked_streams, header_unblocked);
  STAILQ_INIT(&data->pending_blocks);
  return self;
}

VALUE lsqpackrb_dec_feed_encoder(VALUE self, VALUE data) {
  DecoderObj* dec;
  TypedData_Get_Struct(self, DecoderObj, &decoder_data_type, dec);
  struct header_block* hblock;
  Check_Type(data, T_STRING);
  unsigned char* feed_data;
  feed_data = StringValuePtr(data);
  size_t feed_data_len;
  feed_data_len = RSTRING_LEN(data);

  if (lsqpack_dec_enc_in(&dec->dec, feed_data, feed_data_len) < 0) {
    rb_raise(rb_eEncoderStreamErr, "lsqpack_dec_enc_in failed");
    return NULL;
  }

  VALUE list = rb_ary_new();
  STAILQ_FOREACH(hblock, &dec->pending_blocks, entries) {
    if (!hblock->blocked) {
      VALUE val = LONG2NUM(hblock->stream_id);
      rb_ary_push(list, val);
    }
  }
  return list;
}

VALUE lsqpackrb_dec_feed_header(VALUE self, VALUE stream_id, VALUE data) {
  DecoderObj* dec;
  TypedData_Get_Struct(self, DecoderObj, &decoder_data_type, dec);
  uint64_t strm_id;
  size_t dec_len = DECODER_BUF_SIZE;
  unsigned char* feed_data;
  int feed_data_len;
  enum lsqpack_read_header_status status;
  struct header_block* hblock;

  strm_id = FIX2LONG(stream_id);
  Check_Type(data, T_STRING);
  feed_data = StringValuePtr(data);
  feed_data_len = RSTRING_LEN(data);

  STAILQ_FOREACH(hblock, &dec->pending_blocks, entries) {
    if (hblock->stream_id == strm_id) {
      rb_raise(rb_eError, "a header block for stream %d already exists", strm_id);
      return NULL;
    }
  }
  hblock = header_block_new(strm_id, feed_data, feed_data_len);
  status = lsqpack_dec_header_in(&dec->dec, hblock, strm_id, hblock->data_len, &hblock->data_ptr, hblock->data_len,
                                 &hblock->hlist, dec->decoder_buf, &dec_len);
  if (status == LQRHS_BLOCKED || status == LQRHS_NEED) {
    hblock->blocked = 1;
    STAILQ_INSERT_TAIL(&dec->pending_blocks, hblock, entries);
    rb_raise(rb_eStreamBlocked, "stream %d is blocked", strm_id);
    return NULL;
  } else if (status != LQRHS_DONE) {
    rb_raise(rb_eDecompressionFailed, "lsqpack_dec_header_in for stream %d failed", strm_id);
    header_block_free(hblock);
    return NULL;
  }

  VALUE control = rb_str_new(dec->decoder_buf, dec_len);
  VALUE headers = hlist_to_headers(hblock->hlist);
  header_block_free(hblock);
  VALUE tuple = rb_ary_new_capa(2);
  rb_ary_store(tuple, 0, control);
  rb_ary_store(tuple, 1, headers);
  return tuple;
}

VALUE lsqpackrb_dec_resume_header(VALUE self, VALUE stream_id) {
  DecoderObj* dec;
  TypedData_Get_Struct(self, DecoderObj, &decoder_data_type, dec);
  uint64_t strm_id;
  size_t dec_len = DECODER_BUF_SIZE;
  enum lsqpack_read_header_status status;
  struct header_block* hblock;
  int found = 0;

  Check_Type(stream_id, T_FIXNUM);
  strm_id = NUM2LONG(stream_id);

  STAILQ_FOREACH(hblock, &dec->pending_blocks, entries) {
    if (hblock->stream_id == strm_id) {
      found = 1;
      break;
    }
  }
  if (!found) {
    rb_raise(rb_eError, "no pending header block for stream %d", strm_id);
    return NULL;
  }
  if (hblock->blocked) {
    status = LQRHS_BLOCKED;
  } else {
    status = lsqpack_dec_header_read(&dec->dec, hblock, &hblock->data_ptr,
                                     hblock->data_len - (hblock->data_ptr - hblock->data), &hblock->hlist,
                                     dec->decoder_buf, &dec_len);
  }

  if (status == LQRHS_BLOCKED || status == LQRHS_NEED) {
    hblock->blocked = 1;
    rb_raise(rb_eStreamBlocked, "stream %d is blocked", strm_id);
    return NULL;
  } else if (status != LQRHS_DONE) {
    rb_raise(rb_eDecompressionFailed, "lsqpack_dec_header_in for stream %d failed (%d)", strm_id, status);
    STAILQ_REMOVE(&dec->pending_blocks, hblock, header_block, entries);
    header_block_free(hblock);
    return NULL;
  }
  VALUE control = rb_str_new(dec->decoder_buf, dec_len);
  VALUE headers = hlist_to_headers(hblock->hlist);
  VALUE tuple = rb_ary_new_capa(2);
  rb_ary_store(tuple, 0, control);
  rb_ary_store(tuple, 1, headers);
  return tuple;
}

typedef struct {
  struct lsqpack_enc enc;
  unsigned char header_buf[HEADER_BUF_SIZE];
  unsigned char encoder_buf[ENCODER_BUF_SIZE];
  unsigned char prefix_buf[PREFIX_MAX_SIZE];
} EncoderObj;

size_t lsqpackrb_enc_size(EncoderObj* data) { return sizeof(EncoderObj); }

void lsqpackrb_enc_free(EncoderObj* data) {
  lsqpack_enc_cleanup(&data->enc);
  free(data);
}

static const rb_data_type_t encoder_data_type = {
    .wrap_struct_name = "lsqpack_enc_rb",
    .function =
        {
            .dmark = NULL,
            .dfree = lsqpackrb_enc_free,
            .dsize = lsqpackrb_enc_size,
        },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE lsqpackrb_enc_alloc(VALUE self) {
  EncoderObj* data = malloc(sizeof(EncoderObj));
  return TypedData_Wrap_Struct(self, &encoder_data_type, data);
}

VALUE lsqpackrb_enc_init(VALUE self) {
  EncoderObj* data;
  TypedData_Get_Struct(self, EncoderObj, &encoder_data_type, data);
  lsqpack_enc_preinit(&data->enc, NULL);
  return self;
}

VALUE lsqpackrb_enc_apply_settings(VALUE self, VALUE rb_max_table_capacity, VALUE rb_blocked_streams) {
  EncoderObj* data;
  TypedData_Get_Struct(self, EncoderObj, &encoder_data_type, data);

  unsigned int max_table_capacity = FIX2INT(rb_max_table_capacity);
  unsigned int blocked_streams = FIX2INT(rb_blocked_streams);
  unsigned char stdc_buf[LSQPACK_LONGEST_SDTC];
  size_t stdc_buf_len = sizeof(stdc_buf);

  if (lsqpack_enc_init(&data->enc, NULL, max_table_capacity, max_table_capacity, blocked_streams,
                       LSQPACK_ENC_OPT_STAGE_2, stdc_buf, &stdc_buf_len) != 0) {
    rb_raise(rb_eError, "lsqpack_enc_init failed");
    return NULL;
  }
  return rb_str_new(stdc_buf, stdc_buf_len);
}

VALUE lsqpackrb_enc_encode(VALUE self, VALUE stream_id, VALUE headers) {
  EncoderObj* data;
  TypedData_Get_Struct(self, EncoderObj, &encoder_data_type, data);

  uint64_t str_id = FIX2LONG(stream_id);
  unsigned int sequence_number = 0;
  size_t enc_len, header_len, prefix_len;
  size_t enc_off = 0, header_off = PREFIX_MAX_SIZE, prefix_off = 0;

  Check_Type(headers, T_ARRAY);
  if (rb_array_len(headers) == 0) {
    // no headers present
    return NULL;
  }

  if (lsqpack_enc_start_header(&data->enc, str_id, sequence_number) != 0) {
    rb_raise(rb_eError, "lsqpack_enc_start_header failed");
    return NULL;
  }

  for (int i = 0; i < rb_array_len(headers); i++) {
    VALUE entry = rb_ary_entry(headers, i);
    if (rb_array_len(entry) != 2) {
      // no headers present
      rb_raise(rb_eError, "Each header entry must be include 2 elements");
      return NULL;
    }

    VALUE name_ = rb_ary_entry(entry, 0);
    VALUE value_ = rb_ary_entry(entry, 1);
    char* name = StringValuePtr(name_);
    int name_len = RSTRING_LEN(name_);
    char* value = StringValuePtr(value_);
    int value_len = RSTRING_LEN(value_);

    enc_len = ENCODER_BUF_SIZE - enc_off;
    header_len = HEADER_BUF_SIZE - header_off;
    if (lsqpack_enc_encode(&data->enc, data->encoder_buf + enc_off, &enc_len, data->header_buf + header_off,
                           &header_len, name, name_len, value, value_len, 0) != LQES_OK) {
      rb_raise(rb_eError, "lsqpack_enc_encode failed");
      return NULL;
    }
    enc_off += enc_len;
    header_off += header_len;
  }

  prefix_len = lsqpack_enc_end_header(&data->enc, data->prefix_buf, PREFIX_MAX_SIZE, NULL);
  if (prefix_len <= 0) {
    rb_raise(rb_eError, "lsqpack_enc_end_header failed");
    return NULL;
  }
  prefix_off = PREFIX_MAX_SIZE - prefix_len;
  memcpy(data->header_buf + prefix_off, data->prefix_buf, prefix_len);
  VALUE name = rb_str_new(data->encoder_buf, enc_off);
  VALUE value = rb_str_new(data->header_buf + prefix_off, (int)(header_off - prefix_off));
  VALUE tuple = rb_ary_new_from_args(2, name, value);
  return tuple;
}

VALUE lsqpackrb_enc_feed_decoder(VALUE self, VALUE rb_data) {
  EncoderObj* data;
  TypedData_Get_Struct(self, EncoderObj, &encoder_data_type, data);
  char* feed_data = StringValuePtr(rb_data);
  int feed_data_len = RSTRING_LEN(rb_data);

  if (lsqpack_enc_decoder_in(&data->enc, feed_data, feed_data_len) < 0) {
    rb_raise(rb_eDecoderStreamErr, "lsqpack_enc_decoder_in failed");
    return NULL;
  }
  return NULL;
}

void Init_lsqpack(void) {
  rb_mLsqpack = rb_define_module("Lsqpack");
  rb_eError = rb_define_class_under(rb_mLsqpack, "Error", rb_eStandardError);
  rb_eDecompressionFailed = rb_define_class_under(rb_mLsqpack, "DecompressionFailed", rb_eError);
  rb_eDecoderStreamErr = rb_define_class_under(rb_mLsqpack, "DecoderStreamError", rb_eError);
  rb_eEncoderStreamErr = rb_define_class_under(rb_mLsqpack, "EncoderStreamError", rb_eError);
  rb_eStreamBlocked = rb_define_class_under(rb_mLsqpack, "StreamBlocked", rb_eError);

  // Encoder
  rb_clsEncoder = rb_define_class_under(rb_mLsqpack, "Encoder", rb_cObject);
  rb_define_alloc_func(rb_clsEncoder, lsqpackrb_enc_alloc);
  rb_define_method(rb_clsEncoder, "initialize", lsqpackrb_enc_init, 0);
  rb_define_private_method(rb_clsEncoder, "_apply_settings", lsqpackrb_enc_apply_settings, 2);
  rb_define_method(rb_clsEncoder, "encode", lsqpackrb_enc_encode, 2);
  rb_define_method(rb_clsEncoder, "feed_decoder", lsqpackrb_enc_feed_decoder, 1);

  // Decoder
  rb_clsDecoder = rb_define_class_under(rb_mLsqpack, "Decoder", rb_cObject);
  rb_define_alloc_func(rb_clsDecoder, lsqpackrb_dec_alloc);
  rb_define_method(rb_clsDecoder, "initialize", lsqpackrb_dec_init, -1);
  rb_define_method(rb_clsDecoder, "feed_encoder", lsqpackrb_dec_feed_encoder, 1);
  rb_define_method(rb_clsDecoder, "feed_header", lsqpackrb_dec_feed_header, 2);
  rb_define_method(rb_clsDecoder, "resume_header", lsqpackrb_dec_resume_header, 1);
}
