/*
i960SxChipset_Type103
Copyright (c) 2022-2024, Joshua Scoggins
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string>

#include <iostream>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <filesystem>

void print(const boost::system::error_code&) {
    std::cout << "hello, world" << std::endl;
}
using Path = std::filesystem::path;
int main(int argc, char** argv) {
    try {
        boost::program_options::options_description desc{"Options"};
        //clang-format off
        desc.add_options()
                ("help,h", "Help screen")
                ("port,p", boost::program_options::value<Path>(), "the serial port to connect to");
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
        boost::program_options::notify(vm);
        // clang-format on

        if (vm.count("help")) {
            std::cerr << desc << std::endl;
            return 1;
        }
        Path serialPortPath;
        if (!vm.count("port")) {
            serialPortPath = vm["port"].as<Path>();
        } else {
            std::cerr << "No serial port provided!" << std::endl;
            std::cerr << desc << std::endl;
        }
        boost::asio::io_context io;
        boost::asio::serial_port port(io, serialPortPath.string());

        return 0;
    } catch (const boost::program_options::error& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
