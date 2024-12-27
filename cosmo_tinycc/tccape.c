/*
 * TinyCC Cosmopolitan APE format support
 */

#ifdef CONFIG_COSMO

#include "tcc.h"

/* APE format definitions */
#define APE_MAGIC 0x457f /* APE magic number */
#define APE_VERSION 1
#define APE_FLAGS 0

/* APE header structure */
typedef struct {
    uint16_t magic;      /* APE magic number (0x457f) */
    uint8_t  version;    /* APE version */
    uint8_t  flags;      /* APE flags */
    uint32_t mode;       /* File mode */
    uint64_t text_off;   /* Text section offset */
    uint64_t text_size;  /* Text section size */
    uint64_t data_off;   /* Data section offset */
    uint64_t data_size;  /* Data section size */
    uint64_t bss_size;   /* BSS section size */
    uint64_t entry;      /* Entry point */
} APEHeader;

/* Initialize APE output */
static void tcc_output_ape_init(TCCState *s1)
{
    /* Create APE header section */
    s1->ape_header = new_section(s1, ".ape.header", SHT_PROGBITS, SHF_ALLOC);
    section_ptr_add(s1->ape_header, sizeof(APEHeader));
}

/* Write APE header */
static void tcc_write_ape_header(TCCState *s1, FILE *f)
{
    APEHeader hdr;
    Section *text, *data, *bss;

    /* Get sections */
    text = s1->sections[text_section];
    data = s1->sections[data_section];
    bss = s1->sections[bss_section];

    /* Initialize header */
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = APE_MAGIC;
    hdr.version = APE_VERSION;
    hdr.flags = APE_FLAGS;
    hdr.mode = 0755; /* Default executable mode */

    /* Set section information */
    hdr.text_off = text->sh_offset;
    hdr.text_size = text->data_offset;
    hdr.data_off = data->sh_offset;
    hdr.data_size = data->data_offset;
    hdr.bss_size = bss->data_offset;
    hdr.entry = s1->sections[s1->entry_section]->sh_addr;

    /* Write header */
    fwrite(&hdr, 1, sizeof(hdr), f);
}

/* Output APE executable */
int tcc_output_ape(TCCState *s1, const char *filename)
{
    int ret;
    FILE *f;

    /* Initialize APE output */
    tcc_output_ape_init(s1);

    /* Open output file */
    f = fopen(filename, "wb");
    if (!f) {
        tcc_error("could not write '%s'", filename);
        return -1;
    }

    /* Write APE header */
    tcc_write_ape_header(s1, f);

    /* Write sections */
    ret = tcc_output_file(s1, f);

    /* Close file */
    fclose(f);

    return ret;
}

/* Load APE executable */
int tcc_load_ape_exe(TCCState *s1, int fd, const char *filename)
{
    APEHeader hdr;
    int ret;

    /* Read APE header */
    if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
        return -1;

    /* Check magic */
    if (hdr.magic != APE_MAGIC)
        return -1;

    /* Load sections */
    ret = tcc_load_object_file(s1, fd, 0);

    return ret;
}

#endif /* CONFIG_COSMO */
