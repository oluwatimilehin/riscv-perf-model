// <TraceAnalyser.hpp>
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Inst.hpp"

#include "sparta/log/MessageSource.hpp"
#include "sparta/simulation/Clock.hpp"
#include <sparta/simulation/Clock.hpp>

#include "decode/MavisUnit.hpp"
#include "InstSequence.hpp"
#include "InstGenerator.hpp"

// TODO: Replace std::shared_ptr with spartasharedpointer for better performance
namespace olympia
{
    class TraceAnalyser
    {
      public:
        TraceAnalyser(sparta::log::MessageSource & info_logger, MavisType* mavis_facade,
                      sparta::Clock* clk) :
            info_logger_(info_logger),
            mavis_facade_(mavis_facade),
            clk_(clk)
        {
        }

        std::vector<std::shared_ptr<InstSequence>> analyse(const std::string & filename,
                                                           const uint32_t sequence_length,
                                                           const bool skip_nonuser_mode,
                                                           const bool write_json);

      private:
        void writeJSONResults_(const uint32_t sequence_length,
                               const std::filesystem::path & result_file_path,
                               const std::vector<std::shared_ptr<InstSequence>> & sequences,
                               const std::vector<InstPtr> & instructions);

        sparta::log::MessageSource & info_logger_;
        MavisType* mavis_facade_ = nullptr;
        sparta::Clock* clk_ = nullptr;
        std::string results_dir = "statistical-traces";
        std::unique_ptr<InstGenerator> inst_generator_;
    };
} // namespace olympia