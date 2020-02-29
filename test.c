#include <stdint.h>
#include <mz_compat.h>
#include <stdio.h>
#include <mz_os.h>

int main()
{
	zipFile _zip;
	zip_fileinfo zipFileInfo = {0};
	FILE *input = NULL;
	char link_path[256];
	int32_t err = MZ_OK;	
   	zipFileInfo.store_links = 1;
   	zipFileInfo.follow_links = 1;
	
	_zip = zipOpen("test.zip", 0); //0-creat
	
	//zip source.txt
	input = fopen("source.txt", "r");
	if (NULL == input) {
		return -1;
	}
		
	void *buffer = malloc(1024);
	if (buffer == NULL)
	{
		fclose(input);
		return -1;
	}   
	zipOpenNewFileInZip5(_zip, "source.txt", &zipFileInfo, NULL, 0, NULL, 0, NULL, 0, MZ_COMPRESS_LEVEL_DEFAULT, 0, 0, 0, 0, NULL, 0, MZ_VERSION_MADEBY, 0, 0);
	while (!feof(input) && !ferror(input))
	{
		unsigned int len = (unsigned int)fread(buffer, 1, 1024, input);
		zipWriteInFileInZip(_zip, buffer, len);
	}
	free(buffer);
	fclose(input);
	
	//zip link.txt
	//link.txt --> source.txt
	input = fopen("link.txt", "r");
	if (NULL == input) 
	{
		return -1;
	}	
	zipOpenNewFileInZip5(_zip, "link.txt", &zipFileInfo, NULL, 0, NULL, 0, NULL, 0, MZ_COMPRESS_LEVEL_DEFAULT, 0, 0, 0, 0, NULL, 0, MZ_VERSION_MADEBY, 0, 0);
	err = mz_os_read_symlink("link.txt", link_path, sizeof(link_path));
	if (err == MZ_OK)
	{
		zipWriteInFileInZip(_zip, link_path, sizeof(link_path));
	} 
	fclose(input);
	
	zipCloseFileInZip(_zip);
	zipClose(_zip, NULL);
	
	return 0;  
}
