#include "TraceAnalyser.hpp"

#include <Inst.hpp>
#include <algorithm>
#include <iterator>
#include <memory>

namespace olympia
{
    std::vector<std::shared_ptr<InstSequence>>
    TraceAnalyser::analyse(const std::string & filename, const uint32_t sequence_length,
                           const bool skip_nonuser_mode, const bool write_json)

    {
        // TODO: We can cache the generated sequences according to the input parameters and use an
        // existing file if there's a match
        inst_generator_ = InstGenerator::createGenerator(info_logger_, mavis_facade_, filename,
                                                         skip_nonuser_mode);

        std::vector<std::shared_ptr<InstSequence>> results;
        std::unordered_map<std::string, std::shared_ptr<InstSequence>> id_to_seq;

        std::vector<InstPtr> current_instruction_sequence;
        current_instruction_sequence.reserve(sequence_length);

        // Initialize the first instruction sequence
        for (size_t i = 0; i < sequence_length; i++)
        {
            current_instruction_sequence[i] =
                inst_generator_->getNextInst(nullptr); // TODO: Pass the clock as well
        }

        std::shared_ptr<InstSequence> first_sequence =
            std::make_shared<InstSequence>(current_instruction_sequence);

        std::shared_ptr<InstSequence> prev_sequence = first_sequence;

        results.push_back(prev_sequence);
        id_to_seq[prev_sequence->getId()] = prev_sequence;

        while (!inst_generator_->isDone())
        {

            std::shift_left(std::begin(current_instruction_sequence),
                            std::end(current_instruction_sequence), 1);
            current_instruction_sequence[sequence_length - 1] =
                inst_generator_->getNextInst(nullptr);

            auto new_sequence = std::make_shared<InstSequence>(current_instruction_sequence);
            auto [it, inserted] = id_to_seq.try_emplace(new_sequence->getId(), new_sequence);

            if (inserted)
            {
                results.push_back(new_sequence);
            }
            prev_sequence->addSuccessor(it->second->getId());
            prev_sequence = it->second;
        }

        if (prev_sequence->getTransitions().empty())
        {
            prev_sequence->addSuccessor(first_sequence->getId());
        }

        return results;
    };
} // namespace olympia