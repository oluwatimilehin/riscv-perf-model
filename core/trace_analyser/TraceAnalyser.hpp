// <TraceAnalyser.hpp>
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "sparta/log/MessageSource.hpp"

#include "decode/MavisUnit.hpp"
#include "InstSequence.hpp"
#include "InstGenerator.hpp"

// TODO: Replace std::shared_ptr with spartasharedpointer for better performance
namespace olympia
{
    class TraceAnalyser
    {
      public:
        TraceAnalyser(sparta::log::MessageSource & info_logger, MavisType* mavis_facade) :
            info_logger_(info_logger),
            mavis_facade_(mavis_facade)

        {
        }

        std::vector<std::shared_ptr<InstSequence>> analyse(const std::string & filename,
                                                           const uint32_t sequence_length,
                                                           const bool skip_nonuser_mode,
                                                           const bool write_json);

      private:
        sparta::log::MessageSource & info_logger_;
        MavisType* mavis_facade_ = nullptr;
        std::unique_ptr<InstGenerator> inst_generator_;
    };
} // namespace olympia