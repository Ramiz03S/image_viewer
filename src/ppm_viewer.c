#include <stdio.h>
#include <SDL2/SDL.h>
#include <assert.h>


Uint32 SDL_Color_to_Uint32(SDL_Color color, SDL_Surface * psurface){
    return SDL_MapRGBA(psurface->format, color.r, color.g, color.b, color.a);
}

int main(){
    int width, length;
    char buff[71];

    fgets(buff, 71, stdin);    
    assert(strcmp(buff,"P6\n")==0);
    fgets(buff, 71, stdin);
    sscanf(buff,"%d %d", &width, &length);

    fgets(buff, 71, stdin);
    assert(strcmp(buff,"255\n")==0);

    SDL_Window * pwindow = SDL_CreateWindow("image", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, length, 0);
    SDL_Surface * psurface = SDL_GetWindowSurface(pwindow);
    Uint8 red = 0; Uint8 green = 0; Uint8 blue = 0;

    int read_file = 1;
    for(int y = 0; y < length; y++){
        for(int x = 0; x < width; x++){
            red = getchar();
            green = getchar();
            blue = getchar();
            SDL_Color color = {.r = red, .g = green, .b = blue, .a = 1};
            red = blue = green = 0;
            SDL_Rect pixel = {x,y,1,1};
            Uint32 color32 = SDL_Color_to_Uint32(color, psurface);

            SDL_FillRect(psurface,&pixel,color32);
        }        
    }

    SDL_UpdateWindowSurface(pwindow);

    SDL_Event event;
    int simulation_on = 1;
    while(simulation_on){
        while(SDL_PollEvent(&event)){
            switch(event.type){
                case SDL_QUIT:
                    simulation_on = 0;
                
            }
        }
    }
    return 0;
}