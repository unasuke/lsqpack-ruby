# frozen_string_literal: true

require_relative "lsqpack/version"
require_relative "lsqpack/lsqpack"

module Lsqpack
  # class Error < StandardError; end
  # class DecompressonFailed < Error; end
  # class DecoderStreamError < Error; end
  # class EncoderStreamError < Error; end
  # class StreamBlocked < Error; end
  # Your code goes here...

  class Encoder
    # def initialize
    # end

    def apply_settings(max_table_capacity: 0, blocked_streams: 0)
      _apply_settings(max_table_capacity, blocked_streams)
    end
  end

  # class Decoder
  #   def initialize
  #   end

  #   def decode
  #   end
  # end
end
