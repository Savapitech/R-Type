#define ZSTD_STATIC_LINKING_ONLY
#include <brotli/decode.h>
#include <brotli/encode.h>
#include <lz4.h>
#include <lz4hc.h>
#include <lzma.h>
#include <zdict.h>
#include <zlib.h>
#include <zstd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using Bytes = std::vector<std::uint8_t>;
using Clock = std::chrono::steady_clock;

namespace {

constexpr std::size_t MAX_BODY = 529; // MAX_MESSAGE_SIZE of the network layer

struct Algo {
  std::string name;
  std::function<bool(const Bytes &, Bytes &)> compress;
  std::function<bool(const Bytes &, std::size_t, Bytes &)> decompress;
};

std::vector<Bytes> loadCorpus(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::vector<Bytes> packets;
  while (in) {
    std::uint8_t len[2];
    if (!in.read(reinterpret_cast<char *>(len), 2))
      break;
    Bytes body(static_cast<std::size_t>(len[0] | (len[1] << 8)));
    if (!in.read(reinterpret_cast<char *>(body.data()),
                 static_cast<std::streamsize>(body.size())))
      break;
    packets.push_back(std::move(body));
  }
  return packets;
}

Algo deflateAlgo(int level) {
  return {"deflate " + std::to_string(level),
          [level](const Bytes &in, Bytes &out) {
            z_stream s{};
            if (deflateInit2(&s, level, Z_DEFLATED, -15, 8,
                             Z_DEFAULT_STRATEGY) != Z_OK)
              return false;
            out.resize(deflateBound(&s, static_cast<uLong>(in.size())));
            s.next_in = const_cast<Bytef *>(in.data());
            s.avail_in = static_cast<uInt>(in.size());
            s.next_out = out.data();
            s.avail_out = static_cast<uInt>(out.size());
            const int rc = deflate(&s, Z_FINISH);
            out.resize(s.total_out);
            deflateEnd(&s);
            return rc == Z_STREAM_END;
          },
          [](const Bytes &in, std::size_t size, Bytes &out) {
            z_stream s{};
            if (inflateInit2(&s, -15) != Z_OK)
              return false;
            out.resize(size);
            s.next_in = const_cast<Bytef *>(in.data());
            s.avail_in = static_cast<uInt>(in.size());
            s.next_out = out.data();
            s.avail_out = static_cast<uInt>(out.size());
            const int rc = inflate(&s, Z_FINISH);
            inflateEnd(&s);
            return rc == Z_STREAM_END;
          }};
}

Algo lzmaRawAlgo(const Bytes *presetDict = nullptr) {
  auto filters = [presetDict](lzma_filter *f, lzma_options_lzma &opt) {
    lzma_lzma_preset(&opt, 6);
    opt.dict_size = 1u << 12;
    if (presetDict != nullptr) {
      opt.preset_dict = presetDict->data();
      opt.preset_dict_size = static_cast<std::uint32_t>(presetDict->size());
    }
    f[0] = {LZMA_FILTER_LZMA1, &opt};
    f[1] = {LZMA_VLI_UNKNOWN, nullptr};
  };
  return {presetDict ? "LZMA raw + preset dict" : "LZMA raw (lzma1, no header)",
          [filters](const Bytes &in, Bytes &out) {
            lzma_options_lzma opt;
            lzma_filter f[2];
            filters(f, opt);
            out.resize(in.size() + 1024);
            std::size_t pos = 0;
            return lzma_raw_buffer_encode(f, nullptr, in.data(), in.size(),
                                          out.data(), &pos,
                                          out.size()) == LZMA_OK &&
                   (out.resize(pos), true);
          },
          [filters](const Bytes &in, std::size_t size, Bytes &out) {
            lzma_options_lzma opt;
            lzma_filter f[2];
            filters(f, opt);
            out.resize(size);
            std::size_t inPos = 0, outPos = 0;
            return lzma_raw_buffer_decode(f, nullptr, in.data(), &inPos,
                                          in.size(), out.data(), &outPos,
                                          out.size()) == LZMA_OK;
          }};
}

Algo xzAlgo() {
  return {"LZMA xz container",
          [](const Bytes &in, Bytes &out) {
            out.resize(in.size() + 1024);
            std::size_t pos = 0;
            return lzma_easy_buffer_encode(6, LZMA_CHECK_NONE, nullptr,
                                           in.data(), in.size(), out.data(),
                                           &pos, out.size()) == LZMA_OK &&
                   (out.resize(pos), true);
          },
          [](const Bytes &in, std::size_t size, Bytes &out) {
            out.resize(size);
            std::uint64_t memlimit = UINT64_MAX;
            std::size_t inPos = 0, outPos = 0;
            return lzma_stream_buffer_decode(&memlimit, 0, nullptr, in.data(),
                                             &inPos, in.size(), out.data(),
                                             &outPos,
                                             out.size()) == LZMA_OK;
          }};
}

Algo brotliAlgo(int quality) {
  return {"Brotli q" + std::to_string(quality),
          [quality](const Bytes &in, Bytes &out) {
            std::size_t size = BrotliEncoderMaxCompressedSize(in.size());
            out.resize(size);
            if (!BrotliEncoderCompress(quality, 10, BROTLI_MODE_GENERIC,
                                       in.size(), in.data(), &size,
                                       out.data()))
              return false;
            out.resize(size);
            return true;
          },
          [](const Bytes &in, std::size_t size, Bytes &out) {
            out.resize(size);
            std::size_t outSize = size;
            return BrotliDecoderDecompress(in.size(), in.data(), &outSize,
                                           out.data()) ==
                   BROTLI_DECODER_RESULT_SUCCESS;
          }};
}

Algo lz4Algo(bool hc) {
  return {hc ? "LZ4 HC 9" : "LZ4",
          [hc](const Bytes &in, Bytes &out) {
            out.resize(static_cast<std::size_t>(
                LZ4_compressBound(static_cast<int>(in.size()))));
            const int n =
                hc ? LZ4_compress_HC(reinterpret_cast<const char *>(in.data()),
                                     reinterpret_cast<char *>(out.data()),
                                     static_cast<int>(in.size()),
                                     static_cast<int>(out.size()), 9)
                   : LZ4_compress_default(
                         reinterpret_cast<const char *>(in.data()),
                         reinterpret_cast<char *>(out.data()),
                         static_cast<int>(in.size()),
                         static_cast<int>(out.size()));
            if (n <= 0)
              return false;
            out.resize(static_cast<std::size_t>(n));
            return true;
          },
          [](const Bytes &in, std::size_t size, Bytes &out) {
            out.resize(size);
            return LZ4_decompress_safe(
                       reinterpret_cast<const char *>(in.data()),
                       reinterpret_cast<char *>(out.data()),
                       static_cast<int>(in.size()),
                       static_cast<int>(size)) == static_cast<int>(size);
          }};
}

Algo zstdAlgo(int level) {
  return {"zstd " + std::to_string(level),
          [level](const Bytes &in, Bytes &out) {
            out.resize(ZSTD_compressBound(in.size()));
            const std::size_t n = ZSTD_compress(out.data(), out.size(),
                                                in.data(), in.size(), level);
            if (ZSTD_isError(n))
              return false;
            out.resize(n);
            return true;
          },
          [](const Bytes &in, std::size_t size, Bytes &out) {
            out.resize(size);
            return ZSTD_decompress(out.data(), size, in.data(), in.size()) ==
                   size;
          }};
}

Algo zstdDictAlgo(int level, const Bytes &dict, bool tuned) {
  struct Ctx {
    ZSTD_CCtx *cctx = ZSTD_createCCtx();
    ZSTD_DCtx *dctx = ZSTD_createDCtx();
    ZSTD_CDict *cdict;
    ZSTD_DDict *ddict;
  };
  auto ctx = std::make_shared<Ctx>();
  ctx->cdict = ZSTD_createCDict(dict.data(), dict.size(), level);
  ctx->ddict = ZSTD_createDDict(dict.data(), dict.size());
  const std::string label = std::string("zstd ") + std::to_string(level) +
                            " + dict" + (tuned ? " (tuned frame)" : "");
  return {label,
          [ctx, tuned](const Bytes &in, Bytes &out) {
            ZSTD_CCtx_reset(ctx->cctx, ZSTD_reset_session_only);
            ZSTD_CCtx_refCDict(ctx->cctx, ctx->cdict);
            if (tuned) {
              ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_format,
                                     ZSTD_f_zstd1_magicless);
              ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_checksumFlag, 0);
              ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_contentSizeFlag, 0);
              ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_dictIDFlag, 0);
            }
            out.resize(ZSTD_compressBound(in.size()));
            const std::size_t n = ZSTD_compress2(ctx->cctx, out.data(),
                                                 out.size(), in.data(),
                                                 in.size());
            if (ZSTD_isError(n))
              return false;
            out.resize(n);
            return true;
          },
          [ctx, tuned](const Bytes &in, std::size_t size, Bytes &out) {
            ZSTD_DCtx_reset(ctx->dctx, ZSTD_reset_session_only);
            ZSTD_DCtx_refDDict(ctx->dctx, ctx->ddict);
            if (tuned)
              ZSTD_DCtx_setParameter(ctx->dctx, ZSTD_d_format,
                                     ZSTD_f_zstd1_magicless);
            out.resize(size);
            const std::size_t n = ZSTD_decompressDCtx(
                ctx->dctx, out.data(), size, in.data(), in.size());
            return !ZSTD_isError(n) && n == size;
          }};
}

Bytes trainDictionary(const std::vector<Bytes> &train, std::size_t capacity) {
  Bytes samples;
  std::vector<std::size_t> sizes;
  for (const auto &p : train) {
    samples.insert(samples.end(), p.begin(), p.end());
    sizes.push_back(p.size());
  }
  Bytes dict(capacity);
  const std::size_t n = ZDICT_trainFromBuffer(
      dict.data(), dict.size(), samples.data(), sizes.data(),
      static_cast<unsigned>(sizes.size()));
  if (ZDICT_isError(n)) {
    std::cerr << "dictionary training failed: " << ZDICT_getErrorName(n)
              << "\n";
    return {};
  }
  dict.resize(n);
  return dict;
}

void printSizeDistribution(const std::vector<Bytes> &packets) {
  const std::size_t edges[] = {16, 32, 64, 128, 256, MAX_BODY + 1};
  std::size_t counts[6] = {};
  std::size_t total = 0;
  for (const auto &p : packets) {
    total += p.size();
    for (int i = 0; i < 6; ++i)
      if (p.size() < edges[i]) {
        ++counts[i];
        break;
      }
  }
  std::cout << "Corpus: " << packets.size() << " packets, " << total
            << " bytes, mean "
            << (packets.empty() ? 0 : total / packets.size())
            << " bytes/packet\n\n| Body size | Packets |\n| --- | ---: |\n";
  std::size_t lo = 0;
  for (int i = 0; i < 6; ++i) {
    std::cout << "| " << lo << "-" << edges[i] - 1 << " | " << counts[i]
              << " |\n";
    lo = edges[i];
  }
  std::cout << "\n";
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0]
              << " <corpus> [dictionary_out] [dictionary_size]\n";
    return 2;
  }
  auto all = loadCorpus(argv[1]);
  all.erase(std::remove_if(all.begin(), all.end(),
                           [](const Bytes &p) {
                             return p.empty() || p.size() > MAX_BODY;
                           }),
            all.end());
  if (all.size() < 100) {
    std::cerr << "corpus too small (" << all.size()
              << " packets), play longer with RTYPE_CAPTURE_FILE set\n";
    return 1;
  }

  std::vector<Bytes> train, test;
  const std::size_t split = all.size() * 4 / 5;
  for (std::size_t i = 0; i < all.size(); ++i)
    (i < split ? train : test).push_back(all[i]);

  const std::size_t dictSize =
      argc > 3 ? static_cast<std::size_t>(std::atoi(argv[3])) : 8192;
  const Bytes dict = trainDictionary(train, dictSize);
  if (dict.empty())
    return 1;
  if (argc > 2) {
    std::ofstream out(argv[2], std::ios::binary);
    out.write(reinterpret_cast<const char *>(dict.data()),
              static_cast<std::streamsize>(dict.size()));
  }

  printSizeDistribution(all);
  std::cout << "Dictionary: " << dict.size() << " bytes, trained on "
            << train.size() << " packets, tested on " << test.size() << "\n\n";

  // Content part of the zstd dictionary (after its entropy tables), used as a
  // preset dictionary for LZMA so both get the same training data.
  const std::size_t headerSize = ZDICT_getDictHeaderSize(dict.data(), dict.size());
  const Bytes lzmaDict(dict.begin() + (ZDICT_isError(headerSize) ? 0 : headerSize),
                       dict.end());
  std::vector<Algo> algos = {
      lzmaRawAlgo(), lzmaRawAlgo(&lzmaDict),        xzAlgo(),          brotliAlgo(1),
      brotliAlgo(5),        brotliAlgo(11),    deflateAlgo(6),
      deflateAlgo(9),       lz4Algo(false),    lz4Algo(true),
      zstdAlgo(1),          zstdAlgo(3),       zstdAlgo(19),
      zstdDictAlgo(1, dict, false), zstdDictAlgo(3, dict, false),
      zstdDictAlgo(19, dict, false), zstdDictAlgo(1, dict, true),
      zstdDictAlgo(3, dict, true)};

  std::size_t rawTotal = 0;
  for (const auto &p : test)
    rawTotal += p.size();

  std::cout << "| Algorithm | Output / input | With raw fallback | Packets "
               "that grow | Compress (us) | Decompress (us) | Round trip |\n"
               "| --- | ---: | ---: | ---: | ---: | ---: | :---: |\n";
  Bytes comp, back;
  for (auto &algo : algos) {
    std::size_t outTotal = 0, effective = 0, grow = 0;
    double compUs = 0, decompUs = 0;
    bool ok = true;
    for (const auto &p : test) {
      auto t0 = Clock::now();
      if (!algo.compress(p, comp)) {
        ok = false;
        break;
      }
      auto t1 = Clock::now();
      if (!algo.decompress(comp, p.size(), back) || back != p)
        ok = false;
      auto t2 = Clock::now();
      compUs += std::chrono::duration<double, std::micro>(t1 - t0).count();
      decompUs += std::chrono::duration<double, std::micro>(t2 - t1).count();
      outTotal += comp.size();
      effective += std::min(comp.size(), p.size());
      if (comp.size() >= p.size())
        ++grow;
    }
    char line[512];
    std::snprintf(line, sizeof line,
                  "| %s | %.1f %% | %.1f %% | %.1f %% | %.2f | %.2f | %s |\n",
                  algo.name.c_str(), 100.0 * outTotal / rawTotal,
                  100.0 * effective / rawTotal, 100.0 * grow / test.size(),
                  compUs / test.size(), decompUs / test.size(),
                  ok ? "ok" : "FAIL");
    std::cout << line;
  }
  return 0;
}
