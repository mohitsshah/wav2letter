#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include "wav2letter.grpc.pb.h"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <gflags/gflags.h>

#include "inference/decoder/Decoder.h"
#include "inference/examples/AudioToWords.h"
#include "inference/examples/Util.h"
#include "inference/examples/threadpool/ThreadPool.h"
#include "inference/module/feature/feature.h"
#include "inference/module/module.h"
#include "inference/module/nn/nn.h"

using wav2letter::Byte_Stream;
using wav2letter::echo_bytestream;
using wav2letter::Trans_Stream;
using namespace std;

using grpc::ResourceQuota;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

#include <fstream>
#include <vector>

#include "inference/decoder/Decoder.h"
#include "inference/examples/AudioToWords.h"

using namespace w2l;
using namespace w2l::streaming;

DEFINE_string(
    input_files_base_path,
    ".",
    "path is added as prefix to input files unless the input file"
    " is a full path.");
DEFINE_string(
    feature_module_file,
    "feature_extractor.bin",
    "serialized feature extraction module.");
DEFINE_string(
    acoustic_module_file,
    "acoustic_model.bin",
    "binary file containing acoustic module parameters.");
DEFINE_string(
    transitions_file,
    "",
    "binary file containing ASG criterion transition parameters.");
DEFINE_string(tokens_file, "tokens.txt", "text file containing tokens.");
DEFINE_string(lexicon_file, "lexicon.txt", "text file containing lexicon.");
DEFINE_string(silence_token, "_", "the token to use to denote silence");
DEFINE_string(
    language_model_file,
    "language_model.bin",
    "binary file containing language module parameters.");
DEFINE_string(
    decoder_options_file,
    "decoder_options.json",
    "JSON file containing decoder options"
    " including: max overall beam size, max beam for token selection, beam score threshold"
    ", language model weight, word insertion score, unknown word insertion score"
    ", silence insertion score, and use logadd when merging decoder nodes");

std::string GetInputFileFullPath(const std::string& fileName) {
  return GetFullPath(fileName, FLAGS_input_files_base_path);
}

int nTokens;
std::vector<std::string> tokens;
std::shared_ptr<streaming::Sequential> featureModule;
std::shared_ptr<streaming::Sequential> acousticModule;
std::vector<float> transitions;
std::shared_ptr<const DecoderFactory> decoderFactory;
struct w2l::DecoderOptions decoderOptions;
std::shared_ptr<w2l::streaming::Sequential> dnnModule =
    std::make_shared<streaming::Sequential>();

class echo_bytestreamImpl final : public echo_bytestream::Service {
  Status Search(
      ServerContext* context,
      ServerReaderWriter<Trans_Stream, Byte_Stream>* stream) override {
    std::shared_ptr<w2l::streaming::IOBuffer> inputBuffer;
    std::shared_ptr<w2l::streaming::IOBuffer> outputBuffer;
    std::shared_ptr<w2l::streaming::ModuleProcessingState> input;
    int audioSampleCount;

    Byte_Stream data;

    auto decoder = decoderFactory->createDecoder(decoderOptions);

    input = std::make_shared<streaming::ModuleProcessingState>(1);
    inputBuffer = input->buffer(0);

    auto output = dnnModule->start(input);

    outputBuffer = output->buffer(0);

    decoder.start();

    audioSampleCount = 0;
    bool finish_flag = false;
    bool ignore_flag = false;

    while (stream->Read(&data)) {
      std::istringstream inputAudioStream(data.bstream(), std::ios::binary);

      constexpr const int lookBack = 0;
      constexpr const size_t kWavHeaderNumBytes = 44;
      constexpr const float kMaxUint16 = static_cast<float>(0x8000);
      constexpr const int kAudioWavSamplingFrequency = 16000; // 16KHz audio.
      constexpr const int kChunkSizeMsec = 500;

      if (ignore_flag == false) {
        inputAudioStream.ignore(kWavHeaderNumBytes);
        ignore_flag = true;
      }

      const int minChunkSize =
          kChunkSizeMsec * kAudioWavSamplingFrequency / 1000;

      int curChunkSize = readTransformStreamIntoBuffer<int16_t, float>(
          inputAudioStream, inputBuffer, minChunkSize, [](int16_t i) -> float {
            return static_cast<float>(i) / kMaxUint16;
          });

      if (curChunkSize >= minChunkSize) {
        dnnModule->run(input);
        float* data = outputBuffer->data<float>();
        int size = outputBuffer->size<float>();
        if (data && size > 0) {
          decoder.run(data, size);
        }
      }

      if (data.eos() == true) {
        dnnModule->finish(input);
        float* data = outputBuffer->data<float>();
        int size = outputBuffer->size<float>();
        if (data && size > 0) {
          decoder.run(data, size);
        }
        finish_flag = true;
      }

      const int chunk_start_ms =
          (audioSampleCount / (kAudioWavSamplingFrequency / 1000));
      const int chunk_end_ms =
          ((audioSampleCount + curChunkSize) /
           (kAudioWavSamplingFrequency / 1000));

      const std::vector<WordUnit>& wordUnits =
          decoder.getBestHypothesisInWords(lookBack);
      std::string str = "";
      for (const auto& wordUnit : wordUnits) {
        str += wordUnit.word + " ";
      }
      audioSampleCount += curChunkSize;

      Trans_Stream* res_data = new Trans_Stream();
      res_data->set_tstream(str);
      res_data->set_start(chunk_start_ms);
      res_data->set_end(chunk_end_ms);
      stream->Write(*res_data);

      if (finish_flag == true) {
        decoder.finish();
        delete res_data;
        break;
      }

      const int nFramesOut = outputBuffer->size<float>() / nTokens;
      outputBuffer->consume<float>(nFramesOut * nTokens);
      decoder.prune(lookBack);
    }
    return Status::OK;
  }

 private:
  map<long, struct processing_data*> map_multiclient;
};

void RunServer(int argc, char* argv[]) {
  string server_address("0.0.0.0:50051");
  echo_bytestreamImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  unique_ptr<Server> server(builder.BuildAndStart());
  cout << "Server listening on " << server_address << endl;

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // Read files
  {
    TimeElapsedReporter feturesLoadingElapsed("features model file loading");
    std::ifstream featFile(
        GetInputFileFullPath(FLAGS_feature_module_file), std::ios::binary);
    if (!featFile.is_open()) {
      throw std::runtime_error(
          "failed to open feature file=" +
          GetInputFileFullPath(FLAGS_feature_module_file) + " for reading");
    }
    cereal::BinaryInputArchive ar(featFile);
    ar(featureModule);
  }

  {
    TimeElapsedReporter acousticLoadingElapsed("acoustic model file loading");
    std::ifstream amFile(
        GetInputFileFullPath(FLAGS_acoustic_module_file), std::ios::binary);
    if (!amFile.is_open()) {
      throw std::runtime_error(
          "failed to open acoustic model file=" +
          GetInputFileFullPath(FLAGS_feature_module_file) + " for reading");
    }
    cereal::BinaryInputArchive ar(amFile);
    ar(acousticModule);
  }

  {
    TimeElapsedReporter acousticLoadingElapsed("tokens file loading");
    std::ifstream tknFile(GetInputFileFullPath(FLAGS_tokens_file));
    if (!tknFile.is_open()) {
      throw std::runtime_error(
          "failed to open tokens file=" +
          GetInputFileFullPath(FLAGS_tokens_file) + " for reading");
    }
    std::string line;
    while (std::getline(tknFile, line)) {
      tokens.push_back(line);
    }
  }
  nTokens = tokens.size();
  std::cout << "Tokens loaded - " << nTokens << " tokens" << std::endl;

  {
    TimeElapsedReporter decoderOptionsElapsed("decoder options file loading");
    std::ifstream decoderOptionsFile(
        GetInputFileFullPath(FLAGS_decoder_options_file));
    if (!decoderOptionsFile.is_open()) {
      throw std::runtime_error(
          "failed to open decoder options file=" +
          GetInputFileFullPath(FLAGS_decoder_options_file) + " for reading");
    }
    cereal::JSONInputArchive ar(decoderOptionsFile);
    // TODO: factor out proper serialization functionality or Cereal
    // specialization.
    ar(cereal::make_nvp("beamSize", decoderOptions.beamSize),
       cereal::make_nvp("beamSizeToken", decoderOptions.beamSizeToken),
       cereal::make_nvp("beamThreshold", decoderOptions.beamThreshold),
       cereal::make_nvp("lmWeight", decoderOptions.lmWeight),
       cereal::make_nvp("wordScore", decoderOptions.wordScore),
       cereal::make_nvp("unkScore", decoderOptions.unkScore),
       cereal::make_nvp("silScore", decoderOptions.silScore),
       cereal::make_nvp("eosScore", decoderOptions.eosScore),
       cereal::make_nvp("logAdd", decoderOptions.logAdd),
       cereal::make_nvp("criterionType", decoderOptions.criterionType));
  }

  if (!FLAGS_transitions_file.empty()) {
    TimeElapsedReporter acousticLoadingElapsed("transitions file loading");
    std::ifstream transitionsFile(
        GetInputFileFullPath(FLAGS_transitions_file), std::ios::binary);
    if (!transitionsFile.is_open()) {
      throw std::runtime_error(
          "failed to open transition parameter file=" +
          GetInputFileFullPath(FLAGS_transitions_file) + " for reading");
    }
    cereal::BinaryInputArchive ar(transitionsFile);
    ar(transitions);
  }

  // Create Decoder
  {
    TimeElapsedReporter acousticLoadingElapsed("create decoder");
    decoderFactory = std::make_shared<DecoderFactory>(
        GetInputFileFullPath(FLAGS_tokens_file),
        GetInputFileFullPath(FLAGS_lexicon_file),
        GetInputFileFullPath(FLAGS_language_model_file),
        transitions,
        SmearingMode::MAX,
        FLAGS_silence_token,
        0);
  }

  dnnModule->add(featureModule);
  dnnModule->add(acousticModule);

  server->Wait();
}

int main(int argc, char* argv[]) {
  RunServer(argc, argv);

  return 0;
}