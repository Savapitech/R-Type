#define ZSTD_STATIC_LINKING_ONLY // magicless frames are an advanced parameter
#include "Compression.hpp"
#include <memory>
#include <zstd.h>

namespace {

struct Dictionaries {
  ZSTD_CDict *compress;
  ZSTD_DDict *decompress;

  Dictionaries() {
    const auto dictionary = compressionDictionary();
    compress = ZSTD_createCDict(dictionary.data(), dictionary.size(),
                                COMPRESSION_LEVEL);
    decompress = ZSTD_createDDict(dictionary.data(), dictionary.size());
  }
  ~Dictionaries() {
    ZSTD_freeCDict(compress);
    ZSTD_freeDDict(decompress);
  }
};

const Dictionaries &dictionaries() {
  static const Dictionaries instance;
  return instance;
}

struct Contexts {
  ZSTD_CCtx *compress = ZSTD_createCCtx();
  ZSTD_DCtx *decompress = ZSTD_createDCtx();
  ~Contexts() {
    ZSTD_freeCCtx(compress);
    ZSTD_freeDCtx(decompress);
  }
};

Contexts &contexts() {
  thread_local Contexts instance;
  return instance;
}

} // namespace

std::optional<std::vector<std::uint8_t>>
compressBody(const std::vector<std::uint8_t> &body) {
  const auto &dict = dictionaries();
  auto &ctx = contexts();
  if (body.empty() || dict.compress == nullptr || ctx.compress == nullptr)
    return std::nullopt;

  ZSTD_CCtx_reset(ctx.compress, ZSTD_reset_session_only);
  ZSTD_CCtx_refCDict(ctx.compress, dict.compress);
  ZSTD_CCtx_setParameter(ctx.compress, ZSTD_c_format, ZSTD_f_zstd1_magicless);
  ZSTD_CCtx_setParameter(ctx.compress, ZSTD_c_checksumFlag, 0);
  ZSTD_CCtx_setParameter(ctx.compress, ZSTD_c_contentSizeFlag, 0);
  ZSTD_CCtx_setParameter(ctx.compress, ZSTD_c_dictIDFlag, 0);

  std::vector<std::uint8_t> out(ZSTD_compressBound(body.size()));
  const std::size_t written = ZSTD_compress2(
      ctx.compress, out.data(), out.size(), body.data(), body.size());
  if (ZSTD_isError(written) || written >= body.size())
    return std::nullopt;
  out.resize(written);
  return out;
}

bool decompressBody(const std::uint8_t *data, std::size_t size,
                    std::vector<std::uint8_t> &out) {
  const auto &dict = dictionaries();
  auto &ctx = contexts();
  if (size == 0 || dict.decompress == nullptr || ctx.decompress == nullptr)
    return false;

  ZSTD_DCtx_reset(ctx.decompress, ZSTD_reset_session_only);
  ZSTD_DCtx_refDDict(ctx.decompress, dict.decompress);
  ZSTD_DCtx_setParameter(ctx.decompress, ZSTD_d_format, ZSTD_f_zstd1_magicless);

  out.resize(MAX_MESSAGE_SIZE);
  const std::size_t written =
      ZSTD_decompressDCtx(ctx.decompress, out.data(), out.size(), data, size);
  if (ZSTD_isError(written) || written == 0) {
    out.clear();
    return false;
  }
  out.resize(written);
  return true;
}
