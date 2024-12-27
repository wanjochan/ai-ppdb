/*
 * TinyCC Cosmopolitan configuration
 */

#ifndef _COSMO_CONFIG_H
#define _COSMO_CONFIG_H

/* Define if building for Cosmopolitan */
#define CONFIG_COSMO 1

/* Cosmopolitan target */
#define TCC_TARGET_COSMO 1

/* Use APE format */
#define CONFIG_TCC_APE 1

/* System include paths */
#define CONFIG_COSMO_SYSINCLUDEPATHS "/include"

/* Library paths */
#define CONFIG_COSMO_LIBPATHS "/lib"

/* Default library names */
#define CONFIG_COSMO_LIBS "cosmopolitan"

/* Target OS */
#define TCC_TARGET_PE 0

/* Target CPU */
#if defined(__x86_64__)
#define TCC_TARGET_X86_64 1
#elif defined(__i386__)
#define TCC_TARGET_I386 1
#else
#error "Unsupported CPU architecture"
#endif

/* Memory model */
#define PTR_SIZE 8

/* Section alignment */
#define SECTION_ALIGN 4096

/* Stack alignment */
#define STACK_ALIGN 16

/* Page size */
#define TCC_PAGE_SIZE 4096

/* Dynamic linking support */
#define CONFIG_TCC_DYNAMIC 1

/* Backtrace support */
#define CONFIG_TCC_BACKTRACE 1

/* APE format support */
#define CONFIG_TCC_APE 1

/* bcheck support */
#define CONFIG_TCC_BCHECK 1

#endif /* _COSMO_CONFIG_H */
