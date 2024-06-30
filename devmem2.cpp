/*
 * devmem2.c: Simple program to read/write from/to any location in memory.
 *
 *  Copyright (C) 2000, Jan-Derk Bakker (jdb@lartmaker.nl)
 *
 *
 * This software has been developed for the LART computing board
 * (http://www.lart.tudelft.nl/). The development has been sponsored by
 * the Mobile MultiMedia Communications (http://www.mmc.tudelft.nl/)
 * and Ubiquitous Communications (http://www.ubicom.tudelft.nl/)
 * projects.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <cctype>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "CLI/CLI.hpp"

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

struct cmd_mem {
	int fd;
	void *map_base;
    void *virt_addr;
	uint64_t addr;
	uint64_t value;
	char access_type;
	uint32_t number;
};

void read_addr(struct cmd_mem *cmd)
{
	unsigned long read_result;
	uint64_t a;
	uint64_t b;

	for (size_t i = 0; i < cmd->number; i++)
	{
		switch (cmd->access_type)
		{  
			case 'b':
				read_result = *(static_cast<unsigned char*>(cmd->virt_addr) + i);
				break;
			case 'w':
				read_result = *(static_cast<unsigned int*>(cmd->virt_addr) + i);
				break;
			case 'd':
				read_result = *(static_cast<unsigned long*>(cmd->virt_addr) + i);
				break;
			default:
				std::cerr << "Illegal data type '" << cmd->access_type << "'.\n";
				return;
		}

		if (i % 4 == 0)
		{
			b = static_cast<unsigned long>(cmd->addr);
			switch (cmd->access_type)
			{
				case 'b':
					a = b + (i == 0 ? 2 : i - 1) * 2;
					break;
				case 'w': 
					a = b + (i == 0 ? 2 : i - 1) * 4;
					break;
				case 'd':
					a = b + (i == 0 ? 2 : i - 1) * 8;
					break;
				default:
					std::cerr << "Illegal data type '" << cmd->access_type << std::endl;  
					return;  
			}
			if (i == 0) {
				std::cout << std::hex << "0x" << a << ": ";
			}
			else {
				std::cout << std::endl << "0x" << a << ": ";
			}
		}
		switch (cmd->access_type)
		{
			case 'b':
				std::cout << std::setw(2) << std::hex << read_result << " ";
				break;
			case 'w': 
				std::cout << std::setw(8) << std::hex << read_result << " ";
				break;
			case 'd':
				std::cout << std::setw(16) << std::hex << read_result << " ";
				break;
			default:
				std::cerr << "Illegal data type '" << cmd->access_type << "'.\n";  
				return;  
		}
	}
	std::cout << std::endl;
}

void write_addr(struct cmd_mem *cmd)
{
	unsigned long read_result;

	switch (cmd->access_type)
	{
		case 'b':
			*static_cast<unsigned char*>(cmd->virt_addr) = static_cast<unsigned char>(cmd->value);
			read_result = *static_cast<unsigned char*>(cmd->virt_addr);
			break;
		case 'w':
			*static_cast<unsigned int*>(cmd->virt_addr) = static_cast<unsigned int>(cmd->value);
			read_result = *static_cast<unsigned int*>(cmd->virt_addr);
			break;
		case 'd':
			*static_cast<unsigned long*>(cmd->virt_addr) = static_cast<unsigned long>(cmd->value);
			read_result = *static_cast<unsigned long*>(cmd->virt_addr);
			break;
		default:
			std::cerr << "Illegal data type '" << cmd->access_type << "'." << std::endl;
			return;
	}

	switch (cmd->access_type)
	{
		case 'b':
			std::cout << "0x" << std::hex << cmd->addr << ": ";
			std::cout << std::hex << cmd->value << ", ("<< read_result << ")" << std::endl;
			break;
		case 'w': 
			std::cout << "0x" << std::hex << cmd->addr << ": ";
			std::cout << std::hex << cmd->value << ", ("<< read_result << ")" << std::endl;
			break;
		case 'd':
			std::cout << "0x" << std::hex << cmd->addr << ": ";
			std::cout << std::hex << cmd->value << ", ("<< read_result << ")" << std::endl;
			break;
		default:
			std::cerr << "Illegal data type '" << cmd->access_type << "'." << std::endl;
			return;  
	}
}

bool map_open(struct cmd_mem *cmd)
{
    if((cmd->fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) 
	{
		std::cout << "/dev/mem opened failed" << std::endl;
		return false;
	};
    
    /* Map one page */
    cmd->map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, cmd->fd, cmd->addr & ~MAP_MASK);
    if(cmd->map_base == (void *)-1)
	{
		std::cout << "mmap failed" << std::endl;
		return false;
	};
    
    cmd->virt_addr = (uint64_t *)cmd->map_base + (cmd->addr & MAP_MASK);
	return true;
}

void map_close(struct cmd_mem *cmd)
{
	if(munmap(cmd->map_base, MAP_SIZE) == -1) 
	{
		std::cout << "unmap failed" << std::endl;
	}
    close(cmd->fd);
}

int main(int argc, char **argv) {
	bool read, write;
	struct cmd_mem mem = {
		.access_type = 'w',
		.number = 1,
	};

	CLI::App app("");

	app.add_flag("-r", read, "read command");
	app.add_flag("-w", write, "write command");
	app.add_option("-a", mem.addr, "address");
	app.add_option("-n", mem.number, "read number");
	app.add_option("-v", mem.value, "write value");
	app.add_option("-t", mem.access_type, "access_type, byte(b), word(w) or doubleword(d)");

	try 
	{
		app.parse(argc, argv);
	}
	catch (const CLI::ParseError& e) {	return app.exit(e); }

	if (argc == 1)
	{
		std::cout << app.help();
		return 0;
	}
	if ((app.count("-r") && app.count("-a") != 1) || (app.count("-w") && app.count("-a") != 1))
	{
		std::cout << "specify address." << std::endl;
		return 0;
	}
	if (app.count("-w") && app.count("-v") != 1)
	{
		std::cout << "specify value." << std::endl;
		return 0;
	}

	if (map_open(&mem) == false)
	{
		return 0;
	}

	if (app.count("-r"))
	{
		read_addr(&mem);
	}
	if (app.count("-w"))
	{
		write_addr(&mem);
	}

	map_close(&mem);
    return 0;
}
