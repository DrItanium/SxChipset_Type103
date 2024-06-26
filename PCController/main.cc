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
#include <memory>
#include <array>

using Path = std::filesystem::path;
class Device {
    public:
        virtual void read(uint32_t offset, uint8_t* buffer, size_t count) = 0;
        virtual void write(uint32_t offset, const uint8_t* buffer, size_t count) = 0;
};
class RAM : public Device {
    public:
        RAM() : _backingStore(std::make_unique<uint8_t[]>(4ull * 1024ull * 1024ull * 1024ull)) { }
        void read(uint32_t offset, uint8_t* buffer, size_t count) override {
            for (size_t i = offset, j = 0; j < count; ++i, ++j) {
                buffer[j] = _backingStore[i];
            }
        }
        void write(uint32_t offset, const uint8_t* buffer, size_t count) override {
            for (size_t i = offset, j = 0; j < count; ++i, ++j) {
                _backingStore[i] = buffer[j];
            }
        }
    private:
        std::unique_ptr<uint8_t[]> _backingStore;
};
RAM systemRam;
int main(int argc, char** argv) {
    try {
        boost::program_options::options_description desc{"Options"};
        //clang-format off
        desc.add_options()
                ("help,h", "Help screen")
                ("verbose,v", "enable verbose information")
                ("baud,b", boost::program_options::value<unsigned int>()->default_value(115200), "the baud rate of the connection")
                ("flow_control", boost::program_options::value<char>()->default_value('N'), "flow control of the connection, N -> None, S -> Software, H -> Hardware")
                ("parity", boost::program_options::value<char>()->default_value('N'), "parity of the connection, N -> None, O -> Odd, E -> even")
                ("stop_bits", boost::program_options::value<std::string>()->default_value("1"), "number of stop bits, accepted values are 1, 1.5, and 2")
                ("character_size", boost::program_options::value<unsigned int>()->default_value(8), "character size for the connection")
                ("port,p", boost::program_options::value<std::string>(), "the serial port to connect to");
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
        boost::program_options::notify(vm);
        // clang-format on
        bool verbose = vm.count("verbose");
        if (vm.count("help")) {
            std::cerr << desc << std::endl;
            return 1;
        }
        std::string serialPortPath;
        if (vm.count("port")) {
            serialPortPath = vm["port"].as<std::string>();
        } else {
            std::cerr << "No serial port provided!" << std::endl;
            std::cerr << desc << std::endl;
            return 1;
        }

        boost::asio::io_context io;
        boost::asio::serial_port port(io);
        boost::asio::serial_port::baud_rate baud(vm["baud"].as<unsigned int>());
        boost::asio::serial_port::character_size csize(vm["character_size"].as<unsigned int>());
        // parse the flow control settings
        boost::asio::serial_port::flow_control::type controlType;
        switch (vm["flow_control"].as<char>())  {
            case 'N':
            case 'n':
                controlType = decltype(controlType)::none;
                break;
            case 'H':
            case 'h':
                controlType = decltype(controlType)::hardware;
                break;
            case 'S':
            case 's':
                controlType = decltype(controlType)::software;
                break;
            default:
                std::cerr << "Invalid flow control provided!" << std::endl;
                return 1;
        }
        boost::asio::serial_port::flow_control flow(controlType);
        boost::asio::serial_port::parity::type parityType;
        switch (vm["parity"].as<char>()) {
            case 'N':
            case 'n':
                parityType = decltype(parityType)::none;
                break;
            case 'E':
            case 'e':
                parityType = decltype(parityType)::even;
                break;
            case 'O':
            case 'o':
                parityType = decltype(parityType)::odd;
                break;
            default:
                std::cerr << "Invalid parity provided!" << std::endl;
                return 1;
        }
        boost::asio::serial_port::parity parity(parityType);
        boost::asio::serial_port::stop_bits::type sbitsType;
        auto sbitsRaw = vm["stop_bits"].as<std::string>();
        if (sbitsRaw == "1") {
            sbitsType = decltype(sbitsType)::one;
        } else if (sbitsRaw == "1.5") {
            sbitsType = decltype(sbitsType)::onepointfive;
        } else if (sbitsRaw == "2") {
            sbitsType = decltype(sbitsType)::two;
        } else {
            std::cerr << "invalid stop bits size!" << std::endl;
            return 1;
        }
        boost::asio::serial_port::stop_bits sbits(sbitsType);
        try {
            port.open(serialPortPath);
        } catch (const boost::system::system_error& ex) {
            std::cerr << "Could not open " << serialPortPath << std::endl;
            if (verbose) {
                std::cerr << "Actual Message: " << ex.what() << std::endl;
            }
            return 1;
        }
        port.set_option(parity);
        port.set_option(sbits);
        port.set_option(baud);
        port.set_option(csize);
        port.set_option(flow);
        std::cout << "Connection Established to " << serialPortPath << std::endl;
        struct [[gnu::packed]] RequestHeader {
            //uint8_t length;
            uint8_t opcode;
            uint32_t address;
            uint8_t data[16];
        };
        for (;;) {
            std::array<char, 256> buf;
            boost::system::error_code error;
            auto numRead = boost::asio::read(port, boost::asio::buffer(buf, 1), error);
            if (numRead == 0) {
                continue; 
            }
            auto size = static_cast<uint8_t>(buf[0]);
            numRead = boost::asio::read(port, boost::asio::buffer(buf), boost::asio::transfer_exactly(size), error);
            if (error) {
                // An error occurred
            } else {
                RequestHeader header = *reinterpret_cast<RequestHeader*>(buf.data());
                switch (header.opcode) {
                    case 0: {
                                // read 16-byte cache line (send data to the i960)
                                systemRam.read(header.address, header.data, 16);
                                if (verbose) {
                                    std::cout << "opcode: read16" << std::endl;
                                    std::cout << "\t0x" << std::hex << header.address << std::endl;
                                    for (int i = 0; i <  16; ++i) {
                                        std::cout << "\t\t0x" << std::hex << static_cast<int>(header.data[i]) << std::endl;
                                    }
                                }
                                boost::asio::write(port, boost::asio::buffer(header.data));
                            }
                        break;
                    case 1: {
                                // write 16-byte cache line (write data from the i960)
                                if (verbose) {
                                    std::cout << "opcode: write16" << std::endl;
                                    std::cout << "\t0x" << std::hex << header.address << std::endl;
                                    for (int i = 0; i <  16; ++i) {
                                        std::cout << "\t\t0x" << std::hex << static_cast<int>(header.data[i]) << std::endl;
                                    }
                                }
                                systemRam.write(header.address, header.data, 16);
                            }
                        break;
                    case 2:
                        buf[0] = 1;
                        buf[1] = 0x55;
                        std::cout << "New Connection Request Made!" << std::endl;
                        boost::asio::write(port, boost::asio::buffer(buf), boost::asio::transfer_exactly(2));
                        break;
                    default:
                        break;
                }
            }
        }
        return 0;
    } catch (const boost::program_options::error& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
