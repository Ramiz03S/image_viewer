#include <stdio.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <assert.h>
#include <zlib.h>
#define IN_CHUNK 16384
#define OUT_CHUNK 16384
#define SRC_FILE "IDAT_deflated"
#define DST_FILE "IDAT_inflated"

Uint32 generate_32_BE(Uint8 * buff){
    // big endian
    return (Uint32)buff[0] << 24 |
            (Uint32)buff[1] << 16 |
            (Uint32)buff[2] << 8 |
            (Uint32)buff[3];
}

void scan_all_IDAT(FILE* fptr){

    Uint8 length_buff[4];
    Uint8 type_buff[5];
    Uint8 crc_buff[4];
    type_buff[4]='\0';

    int end_of_file = 0;
        while(!end_of_file){
            //processing one chunk
            read(STDIN_FILENO, length_buff, 4);
            read(STDIN_FILENO, type_buff, 4);
            Uint32 data_length = generate_32_BE(length_buff);
            
            if(strcmp(type_buff,"IEND")==0){
                printf("\nEND OF FILE\n");
                read(STDIN_FILENO, crc_buff, 4);
                end_of_file = 1;
                continue;
            }
            if(strcmp(type_buff,"IDAT")==0){

                Uint8 * deflated_data_buff = malloc(data_length);
                read(STDIN_FILENO, deflated_data_buff, data_length);
                fwrite(deflated_data_buff,sizeof(Uint8),data_length,fptr);
                free(deflated_data_buff);
                
            }
            else{
                Uint8 * length_buff = malloc(data_length);
                read(STDIN_FILENO, length_buff, data_length);
                free(length_buff);
            }
            read(STDIN_FILENO, crc_buff, 4);
        }
}

int inflatef(FILE* src, FILE* dst){
    Uint8 inflated_data_buff [OUT_CHUNK];
    Uint8 deflated_data_buff [IN_CHUNK];
    int ret;
    
    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    infstream.avail_in = 0;
    infstream.next_in = Z_NULL; 

    ret = inflateInit(&infstream);
    if (ret !=Z_OK) {
        return ret;
    }

    for(;;){
        if (infstream.avail_in == 0) {
            infstream.avail_in = (uInt)fread(deflated_data_buff,sizeof(Uint8),IN_CHUNK,src);
            if (infstream.avail_in == 0) {
                break;
            }
            infstream.next_in = deflated_data_buff;
        }
        do{
            infstream.avail_out = (uInt)(OUT_CHUNK); 
            infstream.next_out = (Bytef *)inflated_data_buff; 
            
            int ret = inflate(&infstream, Z_NO_FLUSH);
            if ((ret == Z_STREAM_END)) {
                inflateEnd(&infstream);
                return ret;
            }
            if (ret == Z_STREAM_ERROR) {
                inflateEnd(&infstream);
                return ret;
            }
            if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&infstream);
                return ret;
            }
    
            if (infstream.avail_out < OUT_CHUNK){ // some data got inflated that we now have to handle
                size_t bytes_available = (OUT_CHUNK-infstream.avail_out);
                fwrite(inflated_data_buff, sizeof(Uint8),bytes_available,dst);
            }
        } while (infstream.avail_out == 0);
    }


    inflateEnd(&infstream);
    return Z_OK;
}

int main(){

    int ret;

    Uint8 png_sign_buff[8];
    Uint8 png_sign[9] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    read(STDIN_FILENO, png_sign_buff, 8);
    assert(memcmp(png_sign_buff,png_sign,8)==0);

    Uint8 length_buff[4];
    Uint8 type_buff[5];
    Uint8 crc_buff[4];
    type_buff[4]='\0';
    
    // IHDR chunk
    read(STDIN_FILENO, length_buff, 4);
    read(STDIN_FILENO, type_buff, 4);
    Uint32 IHDR_length = generate_32_BE(length_buff);
    Uint8 *IHDR_data_buff = malloc(IHDR_length);
    read(STDIN_FILENO, IHDR_data_buff, 13);
    read(STDIN_FILENO, crc_buff, 4);

    Uint32 width = generate_32_BE(IHDR_data_buff);
    Uint32 height = generate_32_BE(IHDR_data_buff + 4);
    Uint8 bit_depth = IHDR_data_buff[8];
    Uint8 color_type = IHDR_data_buff[9];
    Uint8 compression = IHDR_data_buff[10];
    Uint8 filter_method = IHDR_data_buff[11];
    Uint8 interlace = IHDR_data_buff[12];
    free(IHDR_data_buff);

    FILE* fptr;
    fptr = fopen(SRC_FILE, "wb");
    assert(fptr);
    
    scan_all_IDAT(fptr);
    fclose(fptr);

    FILE* src; FILE* dst;
    src = fopen(SRC_FILE, "rb");
    dst = fopen(DST_FILE, "wb");

    ret = inflatef(src, dst);
    printf("\n%d\n",ret);

    return 0;
}