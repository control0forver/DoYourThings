#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file> <array_name>\n";
        return 1;
    }

    const char* input_filename = argv[1];
    const char* output_filename = argv[2];
    const char* array_name = argv[3];

    // 打开输入文件
    std::ifstream input_file(input_filename, std::ios::binary);
    if (!input_file) {
        std::cerr << "Error: Could not open input file '" << input_filename << "'\n";
        return 1;
    }

    // 读取文件内容到vector
    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(input_file), {});
    input_file.close();

    // 打开输出文件
    std::ofstream output_file(output_filename);
    if (!output_file) {
        std::cerr << "Error: Could not open output file '" << output_filename << "'\n";
        return 1;
    }

    // 写入头文件保护
    output_file << "#pragma once\n\n";
    
    // 写入数组声明
    output_file << "const unsigned char " << array_name << "[] = {\n";

    // 写入十六进制字节
    const int bytes_per_line = 12;
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (i % bytes_per_line == 0) {
            output_file << "    ";
        }
        
        output_file << "0x" << std::hex << std::setw(2) << std::setfill('0') 
                   << static_cast<int>(buffer[i]) << ", ";
        
        if ((i + 1) % bytes_per_line == 0 || i == buffer.size() - 1) {
            output_file << "\n";
        }
    }

    // 写入数组结束和大小信息
    output_file << "};\n\n";
    output_file << "const unsigned int " << array_name << "_size = " 
                << std::dec << buffer.size() << ";\n";

    output_file.close();
    std::cout << "Successfully converted " << buffer.size() 
              << " bytes to C array '" << array_name << "' in " 
              << output_filename << "\n";

    return 0;
}