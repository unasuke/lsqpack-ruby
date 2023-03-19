# frozen_string_literal: true

require "test_helper"

class TestLsqpackEncoder < Minitest::Test
  def test_decoder_stream_error
    encoder = Lsqpack::Encoder.new
    ex = assert_raises Lsqpack::DecoderStreamError do
      encoder.feed_decoder(["\x00"].pack("H*"))
    end
    assert_equal "lsqpack_enc_decoder_in failed", ex.message
  end
end
