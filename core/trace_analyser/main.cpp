#include <boost/program_options/options_description.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <iostream>

#include <boost/program_options.hpp>
#include <string>

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Clock.hpp"

#include "OlympiaAllocators.hpp"

#include "decode/MavisUnit.hpp"
#include "TraceAnalyser.hpp"
#include "InstSequence.hpp"

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    int sequence_length;
    bool skip_nonuser_mode = false;
    bool write_json = true;
    std::string trace_file;

    po::options_description desc("Trace Analyser Options");
    desc.add_options()("help", "Produce help message")(
        "sequence-length,s", po::value<int>(&sequence_length)->required())(
        "skip-non-user", po::bool_switch(&skip_nonuser_mode),
        "For STF traces, skip system instructions if present")("write-json",
                                                               po::bool_switch(&write_json))(
        "trace,t", po::value<std::string>(&trace_file)->required(), "Path to trace file");

    po::positional_options_description positional;
    positional.add("trace", 1);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help"))
    {
        std::cout << desc << "\n";
        return 1;
    }

    po::notify(vm);

    std::cout << "Welcome to the trace analyser \n";
    sparta::TreeNode treenode("trace_analyser", "trace analyser");
    sparta::Scheduler scheduler("scheduler");

    sparta::Clock clock("clock", &scheduler);

    olympia::OlympiaAllocators treenode_alloc(&treenode);
    olympia::MavisUnit::MavisParameters params(&treenode_alloc);
    olympia::MavisUnit mavis_unit(&treenode_alloc, &params);
    olympia::TraceAnalyser analyser(sparta::log::MessageSource::getGlobalDebug(),
                                    mavis_unit.getFacade(), &clock);

    const auto sequences =
        analyser.analyse(trace_file, sequence_length, skip_nonuser_mode, write_json);
    std::cout << "Returned sequence of length: " << sequences.size() << std::endl;
    return 0;
}