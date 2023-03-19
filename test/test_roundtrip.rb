# frozen_string_literal: true

require "test_helper"

class TestRoundtrip < Minitest::Test
  def test_simple
    encoder = Lsqpack::Encoder.new
    decoder = Lsqpack::Decoder.new(max_table_capacity: 0x100, blocked_streams: 0x10)
    stream_id = 0

    # encode headers
    control, data = encoder.encode(stream_id, [["one", "foo"], ["two", "bar"]])
    assert_equal "", control
    assert_equal "\x00\x00*=E\x82\x94\xe7#two\x03bar".b, data

    # decode headers
    decoder.feed_encoder(control)
    control, headers = decoder.feed_header(stream_id, data)
    assert_equal "", control
    assert_equal [["one", "foo"], ["two", "bar"]], headers
  end

  def test_with_settings
    encoder = Lsqpack::Encoder.new
    decoder = Lsqpack::Decoder.new(max_table_capacity: 0x100, blocked_streams: 0x10)
    stream_id = 0

    # apply decoder settings
    tsu = encoder.apply_settings(max_table_capacity: 0x100, blocked_streams: 0x10)
    assert_equal "\x3f\xe1\x01".b, tsu

    # ROUND 1
    # encode headers
    control, data = encoder.encode(stream_id, [["one", "foo"], ["two", "bar"]])
    assert_equal "", control
    assert_equal "\x00\x00*=E\x82\x94\xe7#two\x03bar".b, data

    # decode headers
    decoder.feed_encoder(control)
    control, headers = decoder.feed_header(stream_id, data)
    assert_equal "", control
    assert_equal [["one", "foo"], ["two", "bar"]], headers

    # ROUND 2
    # encode headers
    control, data = encoder.encode(stream_id, [["one", "foo"], ["two", "bar"]])
    assert_equal "b=E\x82\x94\xe7Ctwo\x03bar".b, control
    assert_equal "\x03\x81\x10\x11".b, data

    # decode headers
    decoder.feed_encoder(control)
    control, headers = decoder.feed_header(stream_id, data)
    assert_equal "\x80".b, control
    assert_equal [["one", "foo"], ["two", "bar"]], headers

    # ROUND 3
    # encode headers
    control, data = encoder.encode(stream_id, [["one", "foo"], ["two", "bar"]])
    assert_equal "", control
    assert_equal "\x03\x00\x81\x80".b, data

    # decode headers
    decoder.feed_encoder(control)
    control, headers = decoder.feed_header(stream_id, data)
    assert_equal "\x80".b, control
    assert_equal [["one", "foo"], ["two", "bar"]], headers
  end
end
