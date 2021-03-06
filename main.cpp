#include <assert.h>
#include <vector>


using namespace std;

#include "common.h"
#include "Block.h"
#include "GUI.h"
#include "SpriteMaps.h"
#include "GameBuildings.h"
#include "Constructions.h"
#include "MapLoading.h"
#include "WorldSegment.h"
#include "Creatures.h"
#include "GroundMaterialConfiguration.h"
#include "ContentLoader.h"

#ifdef LINUX_BUILD
#include "stonesense.xpm"
extern void *allegro_icon;
#endif


uint32_t ClockedTime = 0;
uint32_t ClockedTime2 = 0;
uint32_t DebugInt1;

int keyoffset=0;

GameConfiguration config;
bool timeToReloadSegment;
bool timeToReloadConfig;
char currentAnimationFrame;
bool animationFrameShown;

vector<t_matgloss> v_stonetypes;

/*int32_t viewx = 0;
int32_t viewy = 0;
int32_t viewz = 0;
bool followmode = true;*/
volatile int close_button_pressed = FALSE;

void close_button_handler(void)
{
    close_button_pressed = TRUE;
}
END_OF_FUNCTION(close_button_handler)

void WriteErr(char* msg, ...){
  va_list arglist;
  va_start(arglist, msg);
//  char buf[200] = {0};
//  vsprintf(buf, msg, arglist);
  FILE* fp = fopen( "Stonesense.log", "a");
  if(fp)
    vfprintf( fp, msg, arglist );
  va_end(arglist);
  fclose(fp);
}

void LogVerbose(char* msg, ...){
	if (!config.verbose_logging)
		return;
  va_list arglist;
  va_start(arglist, msg);
//  char buf[200] = {0};
//  vsprintf(buf, msg, arglist);
  FILE* fp = fopen( "Stonesense.log", "a");
  if(fp)
    vfprintf( fp, msg, arglist );
  va_end(arglist);
  fclose(fp);
}

void correctBlockForSegmetOffset(int32_t& x, int32_t& y, int32_t& z){
	x -= viewedSegment->x;
	y -= viewedSegment->y; //DisplayedSegmentY;
  z -= viewedSegment->z + viewedSegment->sizez - 2; // loading one above the top of the displayed segment for block rules
}


void benchmark(){
  DisplayedSegmentX = DisplayedSegmentY = 0;
  DisplayedSegmentX = 110; DisplayedSegmentY = 110;DisplayedSegmentZ = 18;
  uint32_t startTime = clock();
  int i = 20;
  while(i--)
	  reloadDisplayedSegment();

  FILE* fp = fopen("benchmark.txt", "w" );
  if(!fp) return;
  fprintf( fp, "%lims", clock() - startTime);
  fclose(fp);
}

void animUpdateProc()
{
	if (animationFrameShown)
	{
		// check before setting, or threadsafety will be borked
		if (currentAnimationFrame >= (MAX_ANIMFRAME-1)) // ie ends up [0 .. MAX_ANIMFRAME)
			currentAnimationFrame = 0;
		else
			currentAnimationFrame = currentAnimationFrame + 1;
		animationFrameShown = false;
	}
}

int main(void)
{
    #ifdef LINUX_BUILD
    allegro_icon = stonesense_xpm;
    #endif
	allegro_init();
	LOCK_FUNCTION(close_button_handler);
    set_close_button_callback(close_button_handler);
  install_keyboard();
  install_mouse();
  enable_hardware_cursor();
  select_mouse_cursor(MOUSE_CURSOR_ARROW);
  WriteErr("\nStonesense launched\n");

  config.debug_mode = false;
  config.hide_outer_blocks = false;
  config.shade_hidden_blocks = true;
  config.load_ground_materials = true;
  config.automatic_reload_time = 0;
  config.automatic_reload_step = 500;
  config.lift_segment_offscreen = 0;
  config.Fullscreen = FULLSCREEN;
  config.screenHeight = RESOLUTION_HEIGHT;
  config.screenWidth = RESOLUTION_WIDTH;
  config.segmentSize.x = DEFAULT_SEGMENTSIZE;
  config.segmentSize.y = DEFAULT_SEGMENTSIZE;
  config.segmentSize.z = DEFAULT_SEGMENTSIZE_Z;
  config.show_creature_names = true;
  config.show_osd = true;
	config.show_intro = true;
  config.track_center = false;
  config.animation_step = 300;
  config.follow_DFscreen = false;
  timeToReloadConfig = true;
  config.fogr = 255;
  config.fogg = 255;
  config.fogb = 255;
  config.foga = 255;
  config.backr = 95;
  config.backg = 95;
  config.backb = 160;
  loadConfigFile();
  set_uformat(U_ASCII);
  if(load_bitmap_font("font.bmp", NULL, NULL))
  	font = load_bitmap_font("font.bmp", NULL, NULL);
  //set debug cursor
  debugCursor.x = config.segmentSize.x / 2;
  debugCursor.y = config.segmentSize.y / 2;

	set_color_depth(32);

  int gfxMode = config.Fullscreen ? GFX_AUTODETECT : GFX_AUTODETECT_WINDOWED;
  if( set_gfx_mode(gfxMode, config.screenWidth, config.screenHeight, 0,0) != 0 ){
    allegro_message("unable to set graphics mode.");
    return 1;
  }

	if (is_windowed_mode()) {
		int res = set_display_switch_mode(SWITCH_BACKGROUND);
		if( res != 0 )
			allegro_message("could not set run in background mode");
	}
  set_alpha_blender();

  loadGraphicsFromDisk();
	if( config.show_intro ){
		//centred splash image
		{
			BITMAP* SplashImage = load_bitmap_withWarning("splash.png");
			masked_blit(SplashImage, screen, 0, 0,
	  			(screen->w - SplashImage->w)/2,
	  			(screen->h - SplashImage->h)/2,
	  			SplashImage->w, SplashImage->h);
			destroy_bitmap(SplashImage);
		}
	  
			textprintf_centre(screen, font, config.screenWidth/2, 50, makecol(255,255,0), "Welcome to Stonesense!");
		textprintf_centre(screen, font, config.screenWidth/2, 60, 0xffffff, "Stonesense is an isometric viewer for Dwarf Fortress.");
		
		textprintf_centre(screen, font, config.screenWidth/2, 80, 0xffffff, "Programming, Jonas Ask and Kris Parker");
		textprintf_centre(screen, font, config.screenWidth/2, 90, 0xffffff, "Lead graphics designer, Dale Holdampf");
		
		textprintf_centre(screen, font, config.screenWidth/2, config.screenHeight-130, 0xffffff, "Contributors:");
		textprintf_centre(screen, font, config.screenWidth/2, config.screenHeight-120, 0xffffff, "7� Nickel, Belal, DeKaFu, Dante, Deon, dyze, Errol, fifth angel,");
		textprintf_centre(screen, font, config.screenWidth/2, config.screenHeight-110, 0xffffff, "frumpton, IDreamOfGiniCoeff, Impaler, Japa, jarathor, ");
		textprintf_centre(screen, font, config.screenWidth/2, config.screenHeight-100, 0xffffff, "Jiri Petru, Lord Nightmare, McMe, Mike Mayday, Nexii ");
		textprintf_centre(screen, font, config.screenWidth/2, config.screenHeight-90, 0xffffff, "Malthus, peterix, Seuss, Talvara, winner, and Xandrin.");

		textprintf_centre(screen, font, config.screenWidth/2, config.screenHeight-70, 0xffffff, "With special thanks to peterix for making dfHack");

		//"The program is in a very early alpha, we're only showcasing it to get ideas and feedback, so use it at your own risk."
		textprintf_centre(screen, font, config.screenWidth/2, config.screenHeight-40, 0xffffff, "Press F9 to continue");
		
		//pause till key is pressed
		while(!key[KEY_F9]) readkey();
		clear( screen );
	}

	//upper left corners
	DisplayedSegmentX = DisplayedSegmentY = DisplayedSegmentZ = 0;

	//ramps
	//DisplayedSegmentX = 238; DisplayedSegmentY = 220;DisplayedSegmentZ = 23;

	//ford. Main hall
	DisplayedSegmentX = 172; DisplayedSegmentY = 195;DisplayedSegmentZ = 15;

  //ford. desert map
  //sDisplayedSegmentX = 78; DisplayedSegmentY = 123;DisplayedSegmentZ = 15;

  DisplayedSegmentX = 125; DisplayedSegmentY = 125;DisplayedSegmentZ = 18;

  //DisplayedSegmentX = 242; DisplayedSegmentY = 345;DisplayedSegmentZ = 15;


  //while(1)
	reloadDisplayedSegment();
	//if(!viewedSegment) return 1;

	// we should have a dfhack attached now, load the config
	/*LoadBuildingConfiguration( &buildingTypes );
	LoadCreatureConfiguration( &creatureTypes );
	LoadGroundMaterialConfiguration( );
  */


	// reload now we have config
	reloadDisplayedSegment();

#ifdef BENCHMARK
  benchmark();
#endif
	install_int( animUpdateProc, config.animation_step );
	initAutoReload();

	paintboard();
	while(!key[KEY_ESC] && !close_button_pressed){
		rest(30);
    if( timeToReloadSegment ){
      reloadDisplayedSegment();
      paintboard();
      timeToReloadSegment = false;
      animationFrameShown = true;
    }
    else if (animationFrameShown == false)
    {
	 	paintboard();
	 	animationFrameShown = true;
    }
		doKeys();
	}
  destroyGraphics();
  flushImgFiles();
  DisconnectFromDF();
  
  //dispose old segment
  if(viewedSegment){
    viewedSegment->Dispose();
	  delete(viewedSegment);
  }
  
	return 0;
}
END_OF_MAIN()


