#include "ape.h"

// APE magic number
#define APE_MAGIC 0x457F464C // ".ELF"

// PE constants
#define IMAGE_DOS_SIGNATURE    0x5A4D // "MZ"
#define IMAGE_NT_SIGNATURE     0x00004550 // "PE\0\0"
#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_FILE_EXECUTABLE_IMAGE 0x0002
#define IMAGE_FILE_DLL 0x2000
#define IMAGE_FILE_LARGE_ADDRESS_AWARE 0x0020
#define IMAGE_SUBSYSTEM_WINDOWS_CUI 0x0003
#define IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE 0x0040
#define IMAGE_DLLCHARACTERISTICS_NX_COMPAT 0x0100
#define IMAGE_DLLCHARACTERISTICS_NO_SEH 0x0400
#define IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE 0x8000
#define IMAGE_SCN_CNT_CODE 0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_MEM_READ 0x40000000
#define IMAGE_SCN_MEM_WRITE 0x80000000

// PE data directory indices
#define IMAGE_DIRECTORY_ENTRY_EXPORT 0
#define IMAGE_DIRECTORY_ENTRY_IMPORT 1
#define IMAGE_DIRECTORY_ENTRY_RESOURCE 2
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION 3
#define IMAGE_DIRECTORY_ENTRY_SECURITY 4
#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5
#define IMAGE_DIRECTORY_ENTRY_DEBUG 6
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE 7
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR 8
#define IMAGE_DIRECTORY_ENTRY_TLS 9
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG 10
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT 11
#define IMAGE_DIRECTORY_ENTRY_IAT 12
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT 13
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14

// PE structures
struct dos_header {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;
};

struct pe_header {
    uint32_t signature;
    uint16_t machine;
    uint16_t number_of_sections;
    uint32_t time_date_stamp;
    uint32_t pointer_to_symbol_table;
    uint32_t number_of_symbols;
    uint16_t size_of_optional_header;
    uint16_t characteristics;
};

struct pe_optional_header {
    uint16_t magic;
    uint8_t major_linker_version;
    uint8_t minor_linker_version;
    uint32_t size_of_code;
    uint32_t size_of_initialized_data;
    uint32_t size_of_uninitialized_data;
    uint32_t address_of_entry_point;
    uint32_t base_of_code;
    uint64_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_operating_system_version;
    uint16_t minor_operating_system_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint32_t win32_version_value;
    uint32_t size_of_image;
    uint32_t size_of_headers;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint64_t size_of_stack_reserve;
    uint64_t size_of_stack_commit;
    uint64_t size_of_heap_reserve;
    uint64_t size_of_heap_commit;
    uint32_t loader_flags;
    uint32_t number_of_rva_and_sizes;
    struct {
        uint32_t virtual_address;
        uint32_t size;
    } data_directory[16];
};

struct pe_section_header {
    char name[8];
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t size_of_raw_data;
    uint32_t pointer_to_raw_data;
    uint32_t pointer_to_relocations;
    uint32_t pointer_to_line_numbers;
    uint16_t number_of_relocations;
    uint16_t number_of_line_numbers;
    uint32_t characteristics;
};

struct pe_export_directory {
    uint32_t characteristics;
    uint32_t time_date_stamp;
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t name;
    uint32_t base;
    uint32_t number_of_functions;
    uint32_t number_of_names;
    uint32_t address_of_functions;
    uint32_t address_of_names;
    uint32_t address_of_name_ordinals;
};

// Global variables
static char* dll_name = "test.dll";
static uint32_t text_section_rva = 0x1000;
static uint32_t edata_section_rva = 0x2000;

// Function implementations
int ape_add_pe_header(void* buf, size_t size) {
    if (!buf || size < sizeof(struct dos_header) + sizeof(struct pe_header)) {
        return -1;
    }

    // DOS header
    struct dos_header* dos = (struct dos_header*)buf;
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = sizeof(struct dos_header);

    // PE header
    struct pe_header* pe = (struct pe_header*)((char*)buf + dos->e_lfanew);
    pe->signature = IMAGE_NT_SIGNATURE;
    pe->machine = IMAGE_FILE_MACHINE_AMD64;
    pe->number_of_sections = 2; // .text and .edata
    pe->characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL | IMAGE_FILE_LARGE_ADDRESS_AWARE;
    pe->size_of_optional_header = sizeof(struct pe_optional_header);

    return 0;
}

int ape_add_pe_sections(void* buf, size_t size) {
    if (!buf || size < sizeof(struct dos_header) + sizeof(struct pe_header) + sizeof(struct pe_optional_header)) {
        return -1;
    }

    struct dos_header* dos = (struct dos_header*)buf;
    struct pe_header* pe = (struct pe_header*)((char*)buf + dos->e_lfanew);
    struct pe_optional_header* opt = (struct pe_optional_header*)((char*)pe + sizeof(struct pe_header));
    struct pe_section_header* sections = (struct pe_section_header*)((char*)opt + sizeof(struct pe_optional_header));

    // Initialize optional header
    opt->magic = 0x20b; // PE32+
    opt->major_linker_version = 1;
    opt->minor_linker_version = 0;
    opt->address_of_entry_point = text_section_rva;
    opt->base_of_code = text_section_rva;
    opt->image_base = 0x180000000;
    opt->section_alignment = 0x1000;
    opt->file_alignment = 0x200;
    opt->major_operating_system_version = 6;
    opt->minor_operating_system_version = 0;
    opt->major_subsystem_version = 6;
    opt->minor_subsystem_version = 0;
    opt->subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
    opt->dll_characteristics = IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE |
                             IMAGE_DLLCHARACTERISTICS_NX_COMPAT |
                             IMAGE_DLLCHARACTERISTICS_NO_SEH |
                             IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE;
    opt->size_of_stack_reserve = 0x100000;
    opt->size_of_stack_commit = 0x1000;
    opt->size_of_heap_reserve = 0x100000;
    opt->size_of_heap_commit = 0x1000;
    opt->number_of_rva_and_sizes = 16;

    // .text section
    strncpy(sections[0].name, ".text", 8);
    sections[0].virtual_address = text_section_rva;
    sections[0].virtual_size = 0x1000;
    sections[0].size_of_raw_data = 0x200;
    sections[0].pointer_to_raw_data = 0x400;
    sections[0].characteristics = IMAGE_SCN_CNT_CODE |
                                IMAGE_SCN_MEM_EXECUTE |
                                IMAGE_SCN_MEM_READ;

    // .edata section
    strncpy(sections[1].name, ".edata", 8);
    sections[1].virtual_address = edata_section_rva;
    sections[1].virtual_size = 0x1000;
    sections[1].size_of_raw_data = 0x200;
    sections[1].pointer_to_raw_data = 0x600;
    sections[1].characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA |
                                IMAGE_SCN_MEM_READ;

    // Update optional header
    opt->size_of_code = sections[0].size_of_raw_data;
    opt->size_of_initialized_data = sections[1].size_of_raw_data;
    opt->size_of_headers = 0x400;
    opt->size_of_image = edata_section_rva + 0x1000;

    // Set export directory
    opt->data_directory[IMAGE_DIRECTORY_ENTRY_EXPORT].virtual_address = edata_section_rva;
    opt->data_directory[IMAGE_DIRECTORY_ENTRY_EXPORT].size = 0x1000;

    printf("Headers size: 0x%x\n", opt->size_of_headers);
    printf("Code base: 0x%x\n", opt->base_of_code);
    printf("Image size: 0x%x\n", opt->size_of_image);

    return 0;
}

int ape_add_pe_exports(void* buf, size_t size) {
    if (!buf || size < sizeof(struct dos_header) + sizeof(struct pe_header) + sizeof(struct pe_optional_header)) {
        return -1;
    }

    struct dos_header* dos = (struct dos_header*)buf;
    struct pe_header* pe = (struct pe_header*)((char*)buf + dos->e_lfanew);
    struct pe_optional_header* opt = (struct pe_optional_header*)((char*)pe + sizeof(struct pe_header));
    struct pe_section_header* sections = (struct pe_section_header*)((char*)opt + sizeof(struct pe_optional_header));

    // Find .edata section
    struct pe_section_header* edata = NULL;
    for (int i = 0; i < pe->number_of_sections; i++) {
        if (strncmp(sections[i].name, ".edata", 8) == 0) {
            edata = &sections[i];
            break;
        }
    }

    if (!edata) {
        return -1;
    }

    // Initialize export directory
    struct pe_export_directory* exports = (struct pe_export_directory*)((char*)buf + edata->pointer_to_raw_data);
    exports->characteristics = 0;
    exports->time_date_stamp = 0;
    exports->major_version = 0;
    exports->minor_version = 0;
    exports->name = edata->virtual_address + sizeof(struct pe_export_directory);
    exports->base = 1;
    exports->number_of_functions = 3;
    exports->number_of_names = 3;
    exports->address_of_functions = edata->virtual_address + 0x100;
    exports->address_of_names = edata->virtual_address + 0x200;
    exports->address_of_name_ordinals = edata->virtual_address + 0x300;

    // Copy DLL name
    char* name_ptr = (char*)exports + sizeof(struct pe_export_directory);
    strcpy(name_ptr, dll_name);

    // Set function addresses
    uint32_t* functions = (uint32_t*)((char*)buf + edata->pointer_to_raw_data + 0x100);
    functions[0] = text_section_rva;
    functions[1] = text_section_rva + 0x100;
    functions[2] = text_section_rva + 0x200;

    // Set function names
    uint32_t* name_rvas = (uint32_t*)((char*)buf + edata->pointer_to_raw_data + 0x200);
    name_rvas[0] = edata->virtual_address + 0x400;
    name_rvas[1] = edata->virtual_address + 0x410;
    name_rvas[2] = edata->virtual_address + 0x420;

    // Set name ordinals
    uint16_t* ordinals = (uint16_t*)((char*)buf + edata->pointer_to_raw_data + 0x300);
    ordinals[0] = 0;
    ordinals[1] = 1;
    ordinals[2] = 2;

    // Copy function names
    char* names = (char*)buf + edata->pointer_to_raw_data + 0x400;
    strcpy(names, "module_main");
    strcpy(names + 0x10, "test_func1");
    strcpy(names + 0x20, "test_func2");

    return 0;
}

int ape_set_dll_name(void* buf, size_t size, const char* name) {
    if (!buf || !name) {
        return -1;
    }
    dll_name = strdup(name);
    return 0;
} 