#include <stdio.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <assert.h>
#include <zlib.h>
#include <math.h>
#define IN_CHUNK 16384
#define OUT_CHUNK 16384
#define DEFLATED_FILE "IDAT_deflated"
#define INFLATED_FILE "IDAT_inflated"
#define UNFILTERED_FILE "IDAT_unfiltered"

Uint32 generate_32_BE(Uint8 * buff){
    // big endian
    return (Uint32)buff[0] << 24 |
            (Uint32)buff[1] << 16 |
            (Uint32)buff[2] << 8 |
            (Uint32)buff[3];
}

Uint8 paeth_predictor(Uint8 left, Uint8 upper, Uint8 upper_left){
    Uint8 p,pl,pu,pul;

    p = left + upper - upper_left;
    pl = (Uint8)abs(p - left);
    pu = (Uint8)abs(p - upper);
    pul = (Uint8)abs(p - upper_left);
    return (pl <= pu && pl <= pul)? left : ((pu <= pul)? upper : upper_left);
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
            printf("%s\n",type_buff);
            if(strcmp(type_buff,"IEND")==0){
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

void unfilter_f(FILE* inflate_src, FILE* unfilter_dst, size_t scanline_length, size_t bytesperpixel){
    
    Uint8 scanline_buff[scanline_length]; // stores a scanline from the uncompressed stream
    Uint8 unfiltered_buff[scanline_length]; // stores the unfiltered stream to be written to the file
    Uint8 prev_scanline_buff[scanline_length]; // stores the previous unfiltered scanline
    Uint8 type_buff[1];
    int ret;

    memset(prev_scanline_buff, 0, scanline_length);
    memset(unfiltered_buff, 0, scanline_length);
    int scanlines_counter = 0;
    int end_of_file = 0;
    while(!end_of_file){

        if(fread(type_buff,sizeof(Uint8),1,inflate_src) != 1){
            end_of_file = 1; 
            continue;
        }
        fread(scanline_buff,sizeof(Uint8),scanline_length,inflate_src);

        switch (type_buff[0]) {
            case '\000':
                fwrite(scanline_buff, 1, scanline_length, unfilter_dst);
                break;
            case '\001':
                // Sub
                //unfilt(x) = filt(x) + unfilt(x−bpp)
                for(int i = 0; i < bytesperpixel; i++){
                    unfiltered_buff[i] = scanline_buff[i] + 0;
                }
                for(int i = bytesperpixel; i < scanline_length; i++){
                    unfiltered_buff[i] = scanline_buff[i] + unfiltered_buff[i - bytesperpixel];
                }
                break;

            case '\002':
                // Up
                // unfilt(x) = filt(x) + Prior(x)
                for(int i = 0; i < scanline_length; i++){
                    unfiltered_buff[i] = scanline_buff[i] + prev_scanline_buff[i] ;
                }
                break;

            case '\003':
                // Average of top and left
                // unfilt(x) = filt(x) + floor((unfilt(x-bpp)+Prior(x))/2)
                for(int i = 0; i < bytesperpixel; i++){
                    unfiltered_buff[i] = scanline_buff[i] + (Uint8)floor(( 0 + prev_scanline_buff[i])/2.0);
                }
                for(int i = bytesperpixel; i < scanline_length; i++){
                    unfiltered_buff[i] = scanline_buff[i] + (Uint8)floor(( unfiltered_buff[i - bytesperpixel] + prev_scanline_buff[i])/2.0);
                }
                break;

            case '\004':
                // Paeth
                for(int i = 0; i < bytesperpixel; i++){
                    unfiltered_buff[i] = scanline_buff[i] + paeth_predictor(0, prev_scanline_buff[i], 0);
                }
                for(int i = bytesperpixel; i < scanline_length; i++){
                    unfiltered_buff[i] = scanline_buff[i] + paeth_predictor(unfiltered_buff[i - bytesperpixel], prev_scanline_buff[i], prev_scanline_buff[i - bytesperpixel]);
                }
                break;
        }
        memcpy(prev_scanline_buff, unfiltered_buff, scanline_length);
        fwrite(unfiltered_buff, 1, scanline_length, unfilter_dst);

        scanlines_counter++;
        
    }
}

Uint32 SDL_Color_to_Uint32(SDL_Color color, SDL_Surface * psurface){
    return SDL_MapRGBA(psurface->format, color.r, color.g, color.b, color.a);
}

void display_img(FILE * unfilter_src, Uint32 width,  Uint32 height, Uint8 bit_depth, Uint8 color_type){
    SDL_Window * pwindow = SDL_CreateWindow("image", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, 0);
    SDL_Surface * psurface = SDL_GetWindowSurface(pwindow);
    Uint8 red = 0; Uint8 green = 0; Uint8 blue = 0; Uint8 alpha = 0;

    if( color_type == 2 && bit_depth == 8){

        for(int y = 0; y < height; y++){
            for(int x = 0; x < width; x++){
                red = getc(unfilter_src);
                green = getc(unfilter_src);
                blue = getc(unfilter_src);
                SDL_Color color = {.r = red, .g = green, .b = blue, .a = 1};
                red = blue = green = 0;
                SDL_Rect pixel = {x,y,1,1};
                Uint32 color32 = SDL_Color_to_Uint32(color, psurface);

                SDL_FillRect(psurface,&pixel,color32);
            }        
        }
    }
    if( color_type == 6 && bit_depth == 8){

        for(int y = 0; y < height; y++){
            for(int x = 0; x < width; x++){
                red = getc(unfilter_src);
                green = getc(unfilter_src);
                blue = getc(unfilter_src);
                alpha = getc(unfilter_src);
                SDL_Color color = {.r = red, .g = green, .b = blue, .a = alpha};
                red = blue = green = 0;
                SDL_Rect pixel = {x,y,1,1};
                Uint32 color32 = SDL_Color_to_Uint32(color, psurface);

                SDL_FillRect(psurface,&pixel,color32);
            }        
        }
    }

    SDL_UpdateWindowSurface(pwindow);
    fclose(unfilter_src);

    SDL_Event event;
    int image_show = 1;
    while(image_show){
        while(SDL_PollEvent(&event)){
            switch(event.type){
                case SDL_QUIT:
                    image_show = 0;
                
            }
        }
    }
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

    // each index reprents the color types possible in a png (0,2,3,4,6)
    // zeros for (1,5) as they're not valid color types
    size_t samples_per_pixel[7] = {1,0,3,1,2,0,4};
    size_t bits_per_pixel = bit_depth * samples_per_pixel[color_type];

    FILE* fptr;
    fptr = fopen(DEFLATED_FILE, "wb");
    assert(fptr);
    
    scan_all_IDAT(fptr);
    fclose(fptr);

    FILE* deflate_src; FILE* inflate_dst;
    deflate_src = fopen(DEFLATED_FILE, "rb");
    inflate_dst = fopen(INFLATED_FILE, "wb");

    ret = inflatef(deflate_src, inflate_dst);

    fclose(deflate_src);
    fclose(inflate_dst);

    // unfiltering
    FILE* unfilter_dst;
    inflate_dst = fopen(INFLATED_FILE, "rb");
    unfilter_dst = fopen(UNFILTERED_FILE, "wb");

    
    
    size_t bytesperpixel = ceil(bits_per_pixel / 8.0);
    size_t scanline_length = width * bytesperpixel;

    unfilter_f(inflate_dst, unfilter_dst, scanline_length, bytesperpixel);

    fclose(inflate_dst);
    fclose(unfilter_dst);

    FILE * unfilter_src;
    unfilter_src = fopen(UNFILTERED_FILE, "rb");
    display_img(unfilter_src, width, height, bit_depth, color_type);

    return 0;
}