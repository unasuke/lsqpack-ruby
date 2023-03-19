# frozen_string_literal: true

require "test_helper"

class TestLsqpackDecoder < Minitest::Test
  def test_blocked_stream
    decoder = Lsqpack::Decoder.new(max_table_capacity: 0x100, blocked_streams: 0x10)
    stream_id = 0

    # no streams unblocked
    assert_equal [], decoder.feed_encoder("")

    # cannot resume non existent block
    assert_raises Lsqpack::Error do
      decoder.resume_header(stream_id)
    end

    # the stream is blocked
    assert_raises Lsqpack::StreamBlocked do
      decoder.feed_header(stream_id, ["0482d9101112"].pack("H*"))
    end

    # the stream id sill blocked
    assert_raises Lsqpack::StreamBlocked do
      decoder.resume_header(stream_id)
    end

    # the stream becomes unblocked
    data = [
      "3fe10168f2b14939d69ce84f8d9635e9ef2a12bd454dc69a659f6cf2b14939d6" \
      "b505b161cc5a9385198fdad313c696dd6d5f4a082a65b6850400bea0837190dc" \
      "138a62d1bf"
    ].pack("H*")
    assert_equal [stream_id], decoder.feed_encoder(data)

    # the header is resumed
    control, headers = decoder.resume_header(stream_id)
    assert_equal "\x80".b, control
    assert_equal [
      [":status", "200"],
      ["x-echo-host", "fb.mvfst.net:4433"],
      ["x-echo-user-agent", "aioquic"],
      ["date", "Sun, 21 Jul 2019 21:31:26 GMT"],
    ], headers
  end

  def test_blocked_stream_free
    skip "How do I explicitly free the object?"
    decoder = Lsqpack::Decoder.new(max_table_capacity: 0x100, blocked_streams: 0x10)
    stream_id = 0

    # the stream is blocked
    assert_raises Lsqpack::StreamBlocked do
      decoder.feed_header(stream_id, ["0482d9101112"].pack("H*"))
    end
  end

  def test_decompression_failed_too_short
    decoder = Lsqpack::Decoder.new(max_table_capacity: 0x100, blocked_streams: 0x10)
    ex = assert_raises Lsqpack::DecompressionFailed do
      decoder.feed_header(0, "")
    end
    assert_equal "lsqpack_dec_header_in for stream 0 failed", ex.message
  end

  def test_decompression_failed_invalid
    decoder = Lsqpack::Decoder.new(max_table_capacity: 0x100, blocked_streams: 0x10)
    ex = assert_raises Lsqpack::DecompressionFailed do
      decoder.feed_header(0, "123")
    end
    assert_equal "lsqpack_dec_header_in for stream 0 failed", ex.message
  end

  def test_encoder_stream_error
    decoder = Lsqpack::Decoder.new(max_table_capacity: 0x100, blocked_streams: 0x10)
    ex = assert_raises Lsqpack::EncoderStreamError do
      decoder.feed_encoder("\x00")
    end
    assert_equal "lsqpack_dec_enc_in failed", ex.message
  end
end
