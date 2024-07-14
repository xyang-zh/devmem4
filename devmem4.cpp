/*
 * devmem Optimus: read/write from/to any location in memory.
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
#include <regex>
#include "CLI/CLI.hpp"

#include <readline/readline.h>
#include <readline/history.h>

using namespace std;

std::vector<std::string> cmd_use_mem = {"use", "mem"};
std::vector<std::string> cmd_use_pci = {"use", "pci", "xxx", "bar", "xxx"};
std::vector<std::string> cmd_xw = {"x", "x/8", "x/16", "x/32", "x/64", "x", "w", "w/8", "w/32", "w/64"};
std::vector<std::string> cmd_help = {"help", "?"};
std::vector<std::string> cmd_leave = {"exit", "quit"};
std::array<std::vector<std::string>*, 5> commands = {
	&cmd_use_mem,
	&cmd_use_pci,
	&cmd_xw,
	&cmd_help,
	&cmd_leave
};

std::string choose {"local"};

struct local {
	#define MAP_SIZE 4096UL
	#define MAP_MASK (MAP_SIZE - 1)
	int fd;
	char filename[100];
	uint64_t addr;
	uint64_t value;
	char access_type;
	uint32_t number;
	/* devmem specified. */
	uint64_t *map_base;
    uint64_t *virt_addr;
};

struct pci {
	int fd;
	char filename[100];
	uint64_t addr;
	uint64_t value;
	char access_type;
	uint32_t number;
	bool opened;
	/* pci specified. */
	unsigned int bar;
	unsigned int domain;
	unsigned int bus;
	unsigned int slot;
	unsigned int function;
	unsigned char *maddr;
	unsigned int size;
	unsigned int offset;
	unsigned int phys;
	unsigned char *bar_addr;
};

void local_read(struct local &cmd)
{
	uint64_t read_result;
	uint64_t addr;

	for (size_t i = 0; i < cmd.number; i++)
	{
		switch (cmd.access_type)
		{  
			case 8:
				addr = reinterpret_cast<uint64_t>(reinterpret_cast<unsigned char*>(cmd.virt_addr) + i);
				read_result = *reinterpret_cast<volatile unsigned char*>(addr);
				break;
			case 16:
				addr = reinterpret_cast<uint64_t>(reinterpret_cast<unsigned short int *>(cmd.virt_addr) + i);
				read_result = *reinterpret_cast<volatile unsigned int*>(addr);
				break;
			case 32:
				addr = reinterpret_cast<uint64_t>(reinterpret_cast<unsigned int *>(cmd.virt_addr) + i);
				read_result = *reinterpret_cast<volatile unsigned int*>(addr);
				break;
			case 64:
				addr = reinterpret_cast<uint64_t>(cmd.virt_addr + i);
				read_result = *reinterpret_cast<volatile unsigned long*>(addr);
				break;
			default:
				std::cerr << "Illegal data type '" << cmd.access_type << "'.\n";
				return;
		}

		if (i % 4 == 0)
		{
			if (i == 0) {
				std::cout << std::hex << "0x" << (cmd.addr + i) << ":    ";
			}
			else {
				std::cout << std::endl << "0x" << (cmd.addr + i) << ":    ";
			}
		}
		switch (cmd.access_type)
		{
			case 8:
				std::cout << "0x" << std::setw(4) << std::hex << std::left << read_result << "    ";
				break;
			case 16: 
				std::cout << "0x" << std::setw(6) << std::hex << std::left << read_result << "    ";
				break;
			case 32:
				std::cout << "0x" << std::setw(8) << std::hex << std::left << read_result << "    ";
				break;
			case 64:
				std::cout << "0x" << std::setw(18) << std::hex << std::left << read_result << "    ";
				break;
			default:
				std::cerr << "Illegal data type " << cmd.access_type << std::endl;
				return;  
		}
	}
	std::cout << std::endl;
}

void local_write(struct local &cmd)
{
	uint64_t read_result;

	switch (cmd.access_type)
	{
		case 8:
			*reinterpret_cast<volatile unsigned char*>(cmd.virt_addr) = static_cast<unsigned char>(cmd.value);
			read_result = *reinterpret_cast<volatile unsigned char*>(cmd.virt_addr);
			break;
		case 16:
			*reinterpret_cast<volatile unsigned short int*>(cmd.virt_addr) = static_cast<unsigned short int>(cmd.value);
			read_result = *reinterpret_cast<volatile unsigned short int*>(cmd.virt_addr);
			break;
		case 32:
			*reinterpret_cast<volatile unsigned int*>(cmd.virt_addr) = static_cast<unsigned int>(cmd.value);
			read_result = *reinterpret_cast<unsigned long*>(cmd.virt_addr);
			break;
		case 64:
			*reinterpret_cast<volatile unsigned long*>(cmd.virt_addr) = static_cast<unsigned long>(cmd.value);
			read_result = *reinterpret_cast<volatile unsigned long*>(cmd.virt_addr);
			break;
		default:
			std::cerr << "Illegal data type '" << cmd.access_type << "'." << std::endl;
			return;
	}

	switch (cmd.access_type)
	{
		case 8:
			std::cout << "0x" << std::hex << cmd.addr << ": ";
			std::cout << "0x" << std::setw(2) << std::hex << cmd.value << ", ("<< read_result << ")" << std::endl;
			break;
		case 16: 
			std::cout << "0x" << std::hex << cmd.addr << ": ";
			std::cout << "0x" << std::setw(4) << std::hex << cmd.value << ", ("<< read_result << ")" << std::endl;
			break;
		case 32:
			std::cout << "0x" << std::hex << cmd.addr << ": ";
			std::cout << "0x" << std::setw(8) << std::hex << cmd.value << ", ("<< read_result << ")" << std::endl;
			break;
		case 64:
			std::cout << "0x" << std::hex << cmd.addr << ": ";
			std::cout << "0x" << std::setw(16) << std::hex << cmd.value << ", ("<< read_result << ")" << std::endl;
			break;
		default:
			std::cerr << "Illegal data type " << cmd.access_type << "'." << std::endl;
			return;  
	}
}

bool local_open(struct local &cmd)
{
    if((cmd.fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) 
	{
		std::cout << "/dev/mem opened failed" << std::endl;
		return false;
	};
    
    /* Map one page */
    cmd.map_base = reinterpret_cast<uint64_t *>(mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, cmd.fd, cmd.addr & ~MAP_MASK));
    if(cmd.map_base == MAP_FAILED)
	{
		std::cout << "mmap failed, " << strerror(errno) <<std::endl;
		return false;
	};
    
    cmd.virt_addr = static_cast<uintptr_t *>(cmd.map_base) + (cmd.addr & MAP_MASK);
	return true;
}

void local_close(struct local &cmd)
{
	if(munmap(cmd.map_base, MAP_SIZE) == -1) 
	{
		std::cout << "unmap failed" << std::endl;
	}
    close(cmd.fd);
}

void use_local()
{
	choose = "local";
}


void pci_read(struct pci &cmd)
{
	unsigned long read_result;
	uint64_t a;
	uint64_t b;
	uint64_t addr;
	uint64_t show_addr;

	for (size_t i = 0; i < cmd.number; i++)
	{
		switch (cmd.access_type)
		{
			case 8:
				addr = reinterpret_cast<uint64_t>(reinterpret_cast<unsigned char *>(cmd.addr) + i);
				read_result = *(static_cast<volatile unsigned char *>(cmd.bar_addr) + addr);
				break;
			case 16:
				addr = reinterpret_cast<uint64_t>(reinterpret_cast<unsigned short int *>(cmd.addr) + i);
				read_result = *(reinterpret_cast<volatile unsigned short int *>(cmd.bar_addr) + addr);
				break;
			case 32:
				addr = reinterpret_cast<uint64_t>(reinterpret_cast<unsigned int *>(cmd.addr) + i);
				read_result = *(reinterpret_cast<volatile unsigned int *>(cmd.bar_addr) + addr);
				break;
			case 64:
				addr = reinterpret_cast<uint64_t>(reinterpret_cast<volatile unsigned long *>(cmd.addr) + i);
				read_result = *(reinterpret_cast<volatile unsigned long *>(cmd.bar_addr) + addr);
				break;
			default:
				std::cerr << "Illegal data type '" << cmd.access_type << std::endl;
				return;
		}

		if (i % 4 == 0)
		{
			if (i == 0) {
				std::cout << std::hex << "0x" << (cmd.addr + i) << ":    ";
			}
			else {
				std::cout << std::endl << "0x" << (cmd.addr + i) << ":    ";
			}
		}
		switch (cmd.access_type)
		{
			case 8:
				std::cout << "0x" << std::setw(2) << std::hex << std::left << read_result << "    ";
				break;
			case 16: 
				std::cout << "0x" << std::setw(4) << std::hex << std::left << read_result << "    ";
				break;
			case 32:
				std::cout << "0x" << std::setw(8) << std::hex << std::left << read_result << "    ";
				break;
			case 64:
				std::cout << "0x" << std::setw(16) << std::hex << std::left << read_result << "    ";
				break;
			default:
				std::cerr << "Illegal data type '" << cmd.access_type << std::endl;  
				return;  
		}
	}
	std::cout << std::endl;
}

void pci_write(struct pci &cmd)
{
	unsigned long read_result;

	switch (cmd.access_type)
	{
		case 8:
			*(reinterpret_cast<volatile unsigned char*>(cmd.bar_addr) + cmd.addr) = static_cast<unsigned char>(cmd.value);
			msync((void *)(cmd.bar_addr + cmd.addr), 1, MS_SYNC | MS_INVALIDATE);
			read_result = *static_cast<volatile unsigned char*>(cmd.bar_addr + cmd.addr);
			break;
		case 16:
			*(reinterpret_cast<volatile unsigned short int*>(cmd.bar_addr) + cmd.addr) = static_cast<unsigned short int>(cmd.value);
			msync((void *)(cmd.bar_addr + cmd.addr), 2, MS_SYNC | MS_INVALIDATE);
			read_result = *(reinterpret_cast<volatile unsigned short int*>(cmd.bar_addr) + cmd.addr);
			break;
		case 32:
			*(reinterpret_cast<volatile unsigned int*>(cmd.bar_addr) + cmd.addr) = static_cast<unsigned int>(cmd.value);
			msync((void *)(cmd.bar_addr + cmd.addr), 4, MS_SYNC | MS_INVALIDATE);
			read_result = *(reinterpret_cast<volatile unsigned long*>(cmd.bar_addr) + cmd.addr);
			break;
		case 64:
			*(reinterpret_cast<volatile unsigned long*>(cmd.bar_addr) + cmd.addr) = static_cast<unsigned long>(cmd.value);
			msync((void *)(cmd.bar_addr + cmd.addr), 8, MS_SYNC | MS_INVALIDATE);
			read_result = *(reinterpret_cast<volatile unsigned long*>(cmd.bar_addr) + cmd.addr);
			break;
		default:
			std::cerr << "Illegal data type '" << cmd.access_type << "'." << std::endl;
			return;
	}

	switch (cmd.access_type)
	{
		case 8:
			std::cout << "0x" << std::hex << cmd.addr << ": ";
			std::cout << std::setw(2) << std::hex << cmd.value << ", ("<< read_result << ")" << std::endl;
			break;
		case 16: 
			std::cout << "0x" << std::hex << cmd.addr << ": ";
			std::cout << "0x" << std::setw(4) << std::hex << cmd.value << ", ("<< read_result << ")" << std::endl;
			break;
		case 32:
			std::cout << "0x" << std::hex << cmd.addr << ": ";
			std::cout << "0x" << std::setw(8) << std::hex << cmd.value << ", ("<< read_result << ")" << std::endl;
			break;
		case 64:
			std::cout << "0x" << std::hex << cmd.addr << ": ";
			std::cout << "0x" << std::setw(16) << std::hex << cmd.value << ", ("<< read_result << ")" << std::endl;
			break;
		default:
			std::cerr << "Illegal data type '" << cmd.access_type << "'." << std::endl;
			return;  
	}
}

bool pci_open(struct pci &dev)
{
	int status;
	struct stat statbuf;
	snprintf(dev.filename, 99, "/sys/bus/pci/devices/%04x:%02x:%02x.%1x/resource%d",
			dev.domain, dev.bus, dev.slot, dev.function, dev.bar);
	dev.fd = open(dev.filename, O_RDWR | O_SYNC);
	if (dev.fd < 0) {
		printf("Open failed for file '%s': errno %d, %s\n",
			dev.filename, errno, strerror(errno));
		return false;
	}

	status = fstat(dev.fd, &statbuf);
	if (status < 0) {
		printf("fstat() failed: errno %d, %s\n",
			errno, strerror(errno));
		return false;
	}
	dev.size = statbuf.st_size;

	/* Map */
	dev.maddr = (unsigned char *)mmap(NULL, (size_t)(dev.size), PROT_READ|PROT_WRITE, MAP_SHARED,
				dev.fd, 0);
	if (dev.maddr == (unsigned char *)MAP_FAILED) {
		printf("BARs that are I/O ports are not supported by this tool\n");
		dev.maddr = 0;
		close(dev.fd);
		return false;
	}

	{
		char configname[100];
		int fd;

		snprintf(configname, 99, "/sys/bus/pci/devices/%04x:%02x:%02x.%1x/config",
				dev.domain, dev.bus, dev.slot, dev.function);
		fd = open(configname, O_RDWR | O_SYNC);
		if (dev.fd < 0) {
			printf("Open failed for file '%s': errno %d, %s\n",
				configname, errno, strerror(errno));
			return -1;
		}

		status = lseek(fd, 0x10 + 4*dev.bar, SEEK_SET);
		if (status < 0) {
			printf("Error: configuration space lseek failed\n");
			close(fd);
			return -1;
		}
		status = read(fd, &dev.phys, 4);
		if (status < 0) {
			printf("Error: configuration space read failed\n");
			close(fd);
			return -1;
		}
		dev.offset = ((dev.phys & 0xFFFFFFF0) % 0x1000);
		dev.bar_addr = (dev.maddr + dev.offset);
		close(fd);
	}

	return true;
}

void pci_close(struct pci &dev)
{
	munmap(dev.maddr, dev.size);
	close(dev.fd);
}

int use_pci(struct pci &_pci)
{
	int opt;		
	int status;
	char *slot = NULL;	
	char *cmdFilePath = NULL;
	struct stat statbuf;

	if (choose == "pci" && _pci.opened)
	{
		return 0;
	}

	snprintf(_pci.filename, 99, "/sys/bus/pci/devices/%04x:%02x:%02x.%1x/resource%d", _pci.domain, _pci.bus, _pci.slot, _pci.function, _pci.bar);
	_pci.fd = open(_pci.filename, O_RDWR | O_SYNC);
	if (_pci.fd < 0) {
		std::cout << "Open failed for file " << _pci.filename << " " << errno << " " << strerror(errno) << std::endl;
		return -1;
	}

	status = fstat(_pci.fd, &statbuf);
	if (status < 0) {
		std::cout << "fstat() failed " << errno << " " << strerror(errno) << std::endl;
		return -1;
	}
	_pci.size = statbuf.st_size;

	_pci.maddr = (unsigned char *)mmap(NULL, (size_t)(_pci.size), PROT_READ|PROT_WRITE, MAP_SHARED, _pci.fd,	0);
	if (_pci.maddr == (unsigned char *)MAP_FAILED) {
		std::cout << "BARs that are I/O ports are not supported by this tool" << std::endl;
		_pci.maddr = 0;
		close(_pci.fd);
		return -1;
	}

	char configname[100];
	int fd;

	snprintf(configname, 99, "/sys/bus/pci/devices/%04x:%02x:%02x.%1x/config",  _pci.domain, _pci.bus, _pci.slot, _pci.function);
	fd = open(configname, O_RDWR | O_SYNC);
	if (_pci.fd < 0) {
		std::cout << "Open failed for file" << configname << errno << strerror(errno) << std::endl;
		return -1;
	}

	status = lseek(fd, 0x10 + 4*_pci.bar, SEEK_SET);
	if (status < 0) {
		std::cout << "Error: configuration space lseek failed" << std::endl;
		close(fd);
		return -1;
	}
	status = read(fd, &_pci.phys, 4);
	if (status < 0) {
		std::cout << "Error: configuration space read failed" << std::endl;
		close(fd);
		return -1;
	}
	_pci.offset = ((_pci.phys & 0xFFFFFFF0) % 0x1000);
	_pci.addr = reinterpret_cast<uint64_t>(_pci.maddr + _pci.offset);
	close(fd);
	_pci.opened = true;

	choose = "pci";

	return 0;
}

void exit_pci(struct pci &_pci)
{
	munmap(_pci.maddr, _pci.size);
	close(_pci.fd);
	_pci.opened = false;
}

struct local local;
struct pci pci;

void read_addr(uint64_t addr, uint32_t width, uint32_t n)
{
	if (choose == "local")
	{
		local.addr = addr;
		local.number = n;
		local.access_type = width;
		if (local_open(local) == false)
		{
			return;
		}

		local_read(local);
	}
	if (choose == "pci")
	{
		pci.addr = addr;
		pci.number = n;
		pci.access_type = width;
		pci_read(pci);
	}
}

void write_addr(uint64_t addr, uint32_t width, uint64_t v)
{
	if (choose == "local")
	{
		local.addr = addr;
		local.value = v;
		local.access_type = width;
		if (local_open(local) == false)
		{
			return;
		}

		local_write(local);
	}
	if (choose == "pci")
	{
		pci.addr = addr;
		pci.value = v;
		pci.access_type = width;
		pci_write(pci);
	}
}


std::vector<string> str_split(string &str)
{
    vector<string> res;
    stringstream input(str);
    string temp;
    while(getline(input, temp, ' '))
    {
        res.push_back(temp);
    }
    return res;
}

void show_help(void)
{
	std::cout << "devmem pro, mem access tools.\n"
		"  choose device: use xxx, choose mem or pci for access\n"
		"      	use mem, chosse /dev/mem for access.\n"
		"      	use pci 03:00.01 bar 0, choose pci device 03:00.01's bar 0 for access.\n"
		"  access command:\n" \
		"  		x 32-bits read.\n" \
		"  		x/8 8-bits read, and so on, up to 64.\n" \
		"  		w 32-bits write.\n" \
		"  		w/8 8-bits write, and so on, up to 64." << std::endl;
}

int process_command(char *cmd)
{
	uint32_t bar;
	std::string use_type;
	std::string pci_slot;
	std::string _cmd {cmd};
	std::vector<std::string> param_list = str_split(_cmd);
	std::vector<std::string> cmds;

	for (auto it = commands.begin(); it != commands.end(); ++it)
	{
		if (std::find((*it)->begin(), (*it)->end(), param_list[0]) == (*it)->end())
		{
			continue;
		}
		else
		{
			cmds.assign((*it)->begin(), (*it)->end());
			break;
		}
	}
	if (cmds.size() == 0)
	{
		std::cout << "no matched commands" << std::endl;
		return 0;
	}

	if (std::find(cmd_help.begin(), cmd_help.end(), param_list[0]) != cmd_help.end())
	{
		show_help();
		return 0;
	}
	else if (param_list[0] == "use")
	{
		if (param_list.size() == 1)
		{
			std::cout << "choose mem or pci\n";
			return 0;
		}

		if (param_list[1] == cmd_use_mem[1])
		{
			use_local();
		}
		else if (param_list[1] == cmd_use_pci[1])
		{
			if (param_list.size() != cmd_use_pci.size())
			{
				std::cout << "need more arguments\n";
				return 0;
			}

			int status;
			status = sscanf(param_list[2].c_str(), "%2x:%2x.%1x", &pci.bus, &pci.slot, &pci.function);
			if (status != 3) {
				std::cout << "Error parsing slot information!" << std::endl;
				return false;
			}
			pci.bar = atoi(param_list[4].c_str());

			use_pci(pci);
		}
		else {
			std::cout << "not support type " << param_list[1] << std::endl;
			return 0;
		}
	}
	else if (std::find(cmd_xw.begin(), cmd_xw.end(), param_list[0]) != cmd_xw.end()) {
		uint64_t addr = 0;
		uint64_t number = 1;
		uint32_t width = 32;
		std::string type = "read";

		if (param_list.size() == 1)
		{
			std::cout << "need address at lease" << param_list[1] << std::endl;
			return 0;
		}

		try
		{
			addr = std::stoull(param_list[1], 0, 16);
		}
		catch(const std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			return 0;
		}
		if (param_list.size() == 3)
		{
			try
			{
				number = std::stoull(param_list[2], 0, 10);
			}
			catch(const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
				return 0;
			}
		}

		if (param_list[0] == "x/8")
		{
			width = 8;
		}
		else if (param_list[0] == "x/16")
		{
			width = 16;
		}
		else if (param_list[0] == "x" || param_list[0] == "x/32")
		{
			width = 32;
		}
		else if (param_list[0] == "x/64")
		{
			width = 64;
		}
		else if (param_list[0] == "w/8")
		{
			width = 8;
			type = "write";
		}
		else if (param_list[0] == "w/16")
		{
			width = 16;
			type = "write";
		}
		else if (param_list[0] == "w" || param_list[0] == "w/32")
		{
			width = 32;
			type = "write";
		}
		else if (param_list[0] == "w/64")
		{
			width = 64;
			type = "write";
		}
		else {
			std::cout << "not support type " << param_list[0] << std::endl;
			return 0;
		}

		if (type == "read")
		{
			read_addr(addr, width, number);
		}
		else {
			write_addr(addr, width, number);
		}
	}
	else if (std::find(cmd_leave.begin(), cmd_leave.end(), param_list[0]) != cmd_leave.end())
	{
		local_close(local);
		pci_close(pci);
		return -1;
	}

	return 0;
}

void cli_run()
{
	char *line;
	int len;
	int status;

	while(1) {
		line = readline("Optimus> ");
		/* Ctrl-D check */
		if (line == NULL) {
			std::cout << std::endl;
			continue;
		}
		/* Empty line check */
		len = strlen(line);
		if (len == 0) {
			continue;
		}
		/* Process the line */
		status = process_command(line);
		if (status < 0) {
			// std::cout << std::endl;
			break;
		}

		/* Add it to the history */
		add_history(line);
		free(line);
	}
	free(line);
	return;
}

int main(int argc, char **argv) {
	cli_run();
    return 0;
}
