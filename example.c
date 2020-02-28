#include "stdint.h"
#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_buf.h"
#include "mz_strm_split.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"

#include <stdio.h>  /* printf */
#include "mz_compat.h"

/***************************************************************************/
zipFile _zip;
typedef struct minizip_opt_s {
    uint8_t     include_path;
    int16_t     compress_level;
    uint8_t     compress_method;
    uint8_t     overwrite;
    uint8_t     append;
    int64_t     disk_size;
    uint8_t     follow_links;
    uint8_t     store_links;
    uint8_t     zip_cd;
    int32_t     encoding;
    uint8_t     verbose;
    uint8_t     aes;
    const char *cert_path;
    const char *cert_pwd;
} minizip_opt;

/***************************************************************************/

int32_t minizip_banner(void);
int32_t minizip_help(void);
int32_t minizip_add_compat(const char *path, const char *password, minizip_opt *options, int32_t arg_count, const char **args);
int32_t writeFileAtPath(const char *fileName, const char *password, minizip_opt *options);
int32_t writeFolderAtPath(char *fileName, const char *passord, minizip_opt *options);

/***************************************************************************/

int32_t minizip_banner(void)
{
    printf("Minizip %s - https://github.com/nmoinvaz/minizip\n", MZ_VERSION);
    printf("---------------------------------------------------\n");
    return MZ_OK;
}

int32_t minizip_help(void)
{
    printf("Usage: minizip [-f][-y][-c cp][-a][-0 to -9][-b|-m][-p pwd] file.zip [files]\n\n" \
           "  -a  Append to existing zip file\n" \
           "  -f  Follow symbolic links\n" \
           "  -y  Store symbolic links\n" \          
           "  -0  Store only\n" \
           "  -1  Compress faster\n" \
           "  -9  Compress better\n" \          
           "  -p  Encryption password\n" \
           "  -s  AES encryption\n" \
           "  -b  BZIP2 compression\n" \
           "  -m  LZMA compression\n\n");
    return MZ_OK;
}

int32_t writeFileAtPath(const char *fileName, const char *password, minizip_opt *options)
{
	int32_t err = MZ_OK;
	FILE *input = fopen(fileName, "r");
	if (NULL == input) {
		return -1;
	}

	zip_fileinfo zipInfo = {0};
	void *buffer = malloc(16384);
	if (buffer == NULL)
	{
		fclose(input);
		return -1;
	}

   	zipInfo.store_links = options->store_links;
   	zipInfo.follow_links = options->follow_links;
   
	zipOpenNewFileInZip5(_zip, fileName, &zipInfo, NULL, 0, NULL, 0, NULL, 0, options->compress_level, 0, 0, 0, 0, password, options->aes, 0, 0, 0);
	
 	if (options->store_links && mz_os_is_symlink(fileName) == MZ_OK)
	{
		char link_path[1024];
		err = mz_os_read_symlink(fileName, link_path, sizeof(link_path));
		if (err == MZ_OK)
			zipWriteInFileInZip(_zip, link_path, sizeof(link_path));
	} else {
		while (!feof(input) && !ferror(input))
		{
			unsigned int len = (unsigned int)fread(buffer, 1, 16384, input);
			zipWriteInFileInZip(_zip, buffer, len);
		}
	}
	zipCloseFileInZip(_zip);
	free(buffer);
	fclose(input);
	return 0;
}

int32_t writeFolderAtPath(char *fileName, const char *passord, minizip_opt *options)
{
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	int32_t err = MZ_OK;
	int16_t is_dir = 0;
	char full_path[1024];
	   
	if (mz_os_is_dir(fileName) == MZ_OK)
		is_dir = 1;
	if (!is_dir || (options->store_links  && mz_os_is_symlink(fileName) == MZ_OK))
	{
		err = writeFileAtPath(fileName, NULL, options);
		return err;
	} else {
		dir = mz_os_open_dir(fileName);
		
		if (dir == NULL)
			return MZ_EXIST_ERROR;
		
		while ((entry = mz_os_read_dir(dir)) != NULL)
		{
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue;
			full_path[0] = 0;
			mz_path_combine(full_path, fileName, sizeof(full_path));
			mz_path_combine(full_path, entry->d_name, sizeof(full_path));
			
			err = writeFolderAtPath(full_path, NULL, options);
			if (err != MZ_OK)
				return err;
		}
	}
	mz_os_close_dir(dir);
	return MZ_OK;
}

int32_t minizip_add_compat(const char *path, const char *password, minizip_opt *options, int32_t arg_count, const char **args)
{
	void *writer = NULL;
	int32_t err = MZ_OK;
	int32_t err_close = MZ_OK;
	int32_t i = 0;
	int16_t is_dir = 0;
	const char *filename_in_zip = NULL;

	
	printf("Archive %s\n", path);
	_zip = zipOpen(path, options->append);
	if(!_zip)
	{
		printf("zipOpen Error\n");
		return -1;
	}

	for (i = 0; i < arg_count; i += 1)
	{
	    filename_in_zip = args[i];
	    printf("args[%d]:%s\n",i,filename_in_zip);
	    //is_dir
	    if (mz_os_is_dir(filename_in_zip) == MZ_OK)
		    is_dir = 1;
	    if (!is_dir)
	    {		 
	    	/* Add file system path to archive */
	    	err = writeFileAtPath(filename_in_zip, password, options); 	
	   } else {
	   	err = writeFolderAtPath(filename_in_zip, password, options);
	   }
	    if (err != MZ_OK)
	      	printf("Error %" PRId32 " adding path to archive %s\n", err, filename_in_zip);
   	}
	zipClose(_zip, NULL);
	
	return err;
}

/***************************************************************************/

#if !defined(MZ_ZIP_NO_MAIN)
int main(int argc, const char *argv[])
{
    minizip_opt options;
    int32_t path_arg = 0;
    int32_t err = 0;
    int32_t i = 0;
    uint8_t do_list = 0;
    uint8_t do_extract = 0;
    uint8_t do_erase = 0;
    const char *path = NULL;
    const char *password = NULL;
    const char *destination = NULL;
    const char *filename_to_extract = NULL;


    minizip_banner();
    if (argc == 1)
    {
        minizip_help();
        return 0;
    }

    memset(&options, 0, sizeof(options));

    options.compress_method = MZ_COMPRESS_METHOD_DEFLATE;
    options.compress_level = MZ_COMPRESS_LEVEL_DEFAULT;

    /* Parse command line options */
    for (i = 1; i < argc; i += 1)
    {
        printf("%s ", argv[i]);
        if (argv[i][0] == '-')
        {
            char c = argv[i][1];   
            if ((c == 'a') || (c == 'A'))
                options.append = 1;           
            else if ((c == 'f') || (c == 'F'))
                options.follow_links = 1;
            else if ((c == 'y') || (c == 'Y'))
                options.store_links = 1;
            else if ((c >= '0') && (c <= '9'))
            {
                options.compress_level = (c - '0');
                if (options.compress_level == 0)
                    options.compress_method = MZ_COMPRESS_METHOD_STORE;
            }
            else if ((c == 'b') || (c == 'B'))
#ifdef HAVE_BZIP2
                options.compress_method = MZ_COMPRESS_METHOD_BZIP2;
#else
                err = MZ_SUPPORT_ERROR;
#endif
            else if ((c == 'm') || (c == 'M'))
#ifdef HAVE_LZMA
                options.compress_method = MZ_COMPRESS_METHOD_LZMA;
#else
                err = MZ_SUPPORT_ERROR;
#endif
            else if ((c == 's') || (c == 'S'))
#ifdef HAVE_WZAES
                options.aes = 1;
#else
                err = MZ_SUPPORT_ERROR;
#endif
            else if (((c == 'h') || (c == 'H')) && (i + 1 < argc))
            {
#ifdef MZ_ZIP_SIGNING
                options.cert_path = argv[i + 1];
                printf("%s ", argv[i + 1]);
#else
                err = MZ_SUPPORT_ERROR;
#endif
                i += 1;
            }          
            else if (((c == 'p') || (c == 'P')) && (i + 1 < argc))
            {
#ifndef MZ_ZIP_NO_ENCRYPTION
                password = argv[i + 1];
                printf("*** ");
#else
                err = MZ_SUPPORT_ERROR;
#endif
                i += 1;
            }
        }
        else if (path_arg == 0)
            path_arg = i;
    }
    printf("\n");

    if (err == MZ_SUPPORT_ERROR)
    {
        printf("Feature not supported\n");
        return err;
    }

    if (path_arg == 0)
    {
        minizip_help();
        return 0;
    }

    path = argv[path_arg];

    err = minizip_add_compat(path, password, &options, argc - (path_arg + 1), &argv[path_arg + 1]);


    return err;
}
#endif
