glitchds source

// Prepare audio files using:
// SOX: sox -V s17.wav -c 1 -2 -r 22050 snd17.raw

// Includes
#include <PA9.h>
#include <fat.h>
#include <sys/dir.h>

#include "gfx/all_gfx.h"
#include "gfx/all_gfx.c"

// Defines

#define GRID_WIDTH 15
#define GRID_HEIGHT 11
#define FREQ_MOD_FADER_HEIGHT 120
#define FREQ_MOD_FADER_CENTER 40

// Modes
#define NUMBER_OF_MODES 7
#define DRAW_CA 1
#define SET_TRIGGERS 2
#define PALLET_MODE 3
#define SET_CLOCK_SOURCE 4
#define FREQ_MOD_MODE 5
#define DISTORTION_MODE 6
#define SAVE_LOAD_MODE 7

// Sequencer Status
#define STOPPED 0
#define CLOCKED 1
#define MANUAL 2

// strumming positions

#define STRUM_TOP_HALF 0
#define STRUM_BOTTOM_HALF 1

#define FREQ_MOD_MULTIPLIER 1000
#define DISTORTION_RANGE 300
#define NUMBER_OF_SOUNDS 30

	// Function prototypes

	void
	choose_swatch(int swatch_number);
void draw8bitRectEx(int x, int y, int width, int height, bool screen, int colour);
void draw_manual_tap_control();
void draw_bpm();
void draw_screen_ca_mode();
void draw_screen_set_clock_source_mode();
void initialize_sounds();
void load_sound_from_file(int slot_id, char *filename);
int find_position_of_filename_in_directory(char *filename);
void update_screen_pallet_mode();

SoundInfo sound_slot[32];

char *filenames[500];
int number_of_files = 0;
int file_offset = 0;

// int slot_assignments[6]; // slot_assignments[slot] = sound_id (for saving snapshots)

char swatch_0_file_assignment[100];
char swatch_1_file_assignment[100];
char swatch_2_file_assignment[100];
char swatch_3_file_assignment[100];
char swatch_4_file_assignment[100];
char swatch_5_file_assignment[100];

int m;
int n;
int neighbors;
int sequencer_step = 0;
int step_rate = 6;
int redraw_flag = 0;
int selected_swatch = 1;
int sequencer_status = STOPPED;
int strum_quadrant;
int distortion_level = 0;
int distortion = 0;
int distortion_y = 0;
int file_id = 0;
int keyboard_visible = 0;

int channel_frequencies[7];
int bpm = 90;
int freq_mod[6][32];
int triggers[GRID_WIDTH][GRID_HEIGHT];
bool now[GRID_WIDTH][GRID_HEIGHT], next[GRID_WIDTH][GRID_HEIGHT];
bool seed[GRID_WIDTH][GRID_HEIGHT];

#define pixel_width 16

void clear_ca(bool mat[][GRID_HEIGHT]) // Sets matrix to all dead
{
	int x;
	int y;

	for (y = 0; y < GRID_HEIGHT; y++)
	{
		for (x = 0; x < GRID_WIDTH; x++)
			mat[x][y] = 0;
	}
}

void clear_triggers(int triggers[][GRID_HEIGHT]) // Sets matrix to all dead
{
	int x;
	int y;

	for (y = 0; y < GRID_HEIGHT; y++)
	{
		for (x = 0; x < GRID_WIDTH; x++)
			triggers[x][y] = 0;
	}
}

void draw8bitRect(int x, int y, int width, int height, bool screen, int colour)
{
	x = x * pixel_width;
	y = y * pixel_width;

	draw8bitRectEx(x, y, width, height, screen, colour);
}

void draw8bitRectEx(int x, int y, int width, int height, bool screen, int colour)
{

	// Draw top line
	u16 lastX = x + width;
	u16 i;
	for (i = x; i < lastX; i++)
	{
		PA_Put8bitPixel(screen, i, y, colour);
	}

	// Calculate DMA copy values - we need to deal with even values only
	u16 dmaX = (x + 1) & ~1;
	s32 dmaWidth = width;
	bool drawRight = false;

	// Did we convert to an even x value?
	if (dmaX != x)
	{
		// Reduce the width to compensate
		dmaWidth--;

		// Remember that we need to draw an extra pixel
		drawRight = true;
	}

	// Pre-emptive bitshift to save calculations
	dmaX = dmaX >> 1;
	dmaWidth = (dmaWidth & 1 ? dmaWidth - 1 : dmaWidth) >> 1;

	// Calculate last line to draw
	u16 lastY = y + height;
	lastX = x + width - 1;

	// Precalculate line values for loop
	u16 *line0 = PA_DrawBg[screen] + dmaX + (y << 7);
	u16 *linei = line0 + (1 << 7);

	// Loop through all lines
	for (i = y + 1; i < lastY; i++)
	{

		// Perform DMA copy
		if (dmaWidth > 0)
			DMA_Copy(line0, linei, dmaWidth, DMA_16NOW);

		// Fill left gap
		if (x & 1)
			PA_Put8bitPixel(screen, x, i, colour);

		// Fill right gap
		if ((width & 1) || (drawRight))
			PA_Put8bitPixel(screen, lastX, i, colour);

		// Move to next line
		linei += (1 << 7);
	}
}

void drawBlock(int x, int y, bool screen, int colour)
{
	draw8bitRect(x, y, pixel_width, pixel_width, screen, colour);
}

void calculate_ca(bool mata[][GRID_HEIGHT], bool matb[][GRID_HEIGHT])
{
	int x;
	int y;

	for (y = 1; y < GRID_HEIGHT - 1; y++)
	{
		for (x = 1; x < GRID_WIDTH - 1; x++)
		{
			neighbors = 0;
			// Begin counting number of neighbors:

			// Begin counting number of neighbors:
			if (mata[x - 1][y - 1] == 1)
				neighbors += 1;
			if (mata[x - 1][y] == 1)
				neighbors += 1;
			if (mata[x - 1][y + 1] == 1)
				neighbors += 1;
			if (mata[x][y - 1] == 1)
				neighbors += 1;
			if (mata[x][y + 1] == 1)
				neighbors += 1;
			if (mata[x + 1][y - 1] == 1)
				neighbors += 1;
			if (mata[x + 1][y] == 1)
				neighbors += 1;
			if (mata[x + 1][y + 1] == 1)
				neighbors += 1;

			// Apply rules to the cell:
			if (mata[x][y] == 1 && neighbors < 2)
				matb[x][y] = 0;
			else if (mata[x][y] == 1 && neighbors > 3)
				matb[x][y] = 0;
			else if (mata[x][y] == 1 && (neighbors == 2 || neighbors == 3))
				matb[x][y] = 1;
			else if (mata[x][y] == 0 && neighbors == 3)
				matb[x][y] = 1;
		}
	}
}

void swap_ca(bool mata[][GRID_HEIGHT], bool matb[][GRID_HEIGHT]) // Replaces first matrix with second
{
	int x;
	int y;

	for (y = 0; y < GRID_HEIGHT; y++)
	{
		for (x = 0; x < GRID_WIDTH; x++)
			mata[x][y] = matb[x][y];
	}
}

void seq_play_audio(bool mata[][GRID_HEIGHT], bool matb[][GRID_HEIGHT]) // Replaces first matrix with second
{
	// Play sounds
	int x;
	int y;
	int channel_number;
	int sound_id;

	// int step_increase = 0;
	// int step_decrease = 0;

	for (y = 0; y < GRID_HEIGHT; y++)
	{
		for (x = 0; x < GRID_WIDTH; x++)
		{
			if ((!mata[x][y]) && matb[x][y])
			{
				if ((triggers[x][y] > 0) && (triggers[x][y] < 7))
				{
					sound_id = triggers[x][y] - 1;
					sound_slot[sound_id].rate = (s32)22050 + ((freq_mod[sound_id][sequencer_step] - FREQ_MOD_FADER_CENTER) * FREQ_MOD_MULTIPLIER);
					channel_number = AS_SoundPlay(sound_slot[sound_id]);
					channel_frequencies[channel_number] = sound_slot[sound_id].rate;
				}
			}
		}
	}
}

void draw_ca(bool mat[][GRID_HEIGHT], bool screen) // Prints matrix to screen
{

	int x;
	int y;

	PA_Clear8bitBg(screen);

	for (y = 1; y < GRID_HEIGHT; y++)
	{
		for (x = 1; x < GRID_WIDTH; x++)
		{
			if (mat[x][y])
			{
				// draw8bitRect(x, y, pixel_width, pixel_width, screen, 1); // int x, int y, int w, int h, bool screen, int color

				drawBlock(x, y, screen, 1);
			}
		}
	}
}

void draw_matrix(int mat[][GRID_HEIGHT], bool screen)
{

	int x;
	int y;

	for (y = 1; y < GRID_HEIGHT; y++)
	{
		for (x = 1; x < GRID_WIDTH; x++)
		{
			if (mat[x][y])
				draw8bitRect(x, y, pixel_width, pixel_width, screen, mat[x][y]); // int x, int y, int w, int h, bool screen, int color
		}
	}
}

void draw_swatches()
{
	draw8bitRect(7, 0, pixel_width - 2, pixel_width - 2, 0, 1);
	draw8bitRect(8, 0, pixel_width - 2, pixel_width - 2, 0, 2);
	draw8bitRect(9, 0, pixel_width - 2, pixel_width - 2, 0, 3);
	draw8bitRect(10, 0, pixel_width - 2, pixel_width - 2, 0, 4);
	draw8bitRect(11, 0, pixel_width - 2, pixel_width - 2, 0, 5);
	draw8bitRect(12, 0, pixel_width - 2, pixel_width - 2, 0, 6);
}

int poll_swatches()
{
	int x;
	int y;

	if (Stylus.Newpress)
	{
		x = Stylus.X;
		y = Stylus.Y;

		if (y < 17)
		{
			if (x >= 112 && x <= 127)
				selected_swatch = 1;
			if (x >= 128 && x <= 143)
				selected_swatch = 2;
			if (x >= 144 && x <= 159)
				selected_swatch = 3;
			if (x >= 160 && x <= 175)
				selected_swatch = 4;
			if (x >= 176 && x <= 191)
				selected_swatch = 5;
			if (x >= 191 && x <= 208)
				selected_swatch = 6;
			draw_swatches();
			choose_swatch(selected_swatch);

			return (1);
		}
	}

	if (Pad.Newpress.Left)
	{
		selected_swatch--;
		if (selected_swatch < 1)
			selected_swatch = 6;
		draw_swatches();
		choose_swatch(selected_swatch);

		return (1);
	}

	if (Pad.Newpress.Right)
	{
		selected_swatch++;
		if (selected_swatch > 6)
			selected_swatch = 1;
		draw_swatches();
		choose_swatch(selected_swatch);

		return (1);
	}

	return (0);
}

void choose_swatch(int swatch_number)
{

	switch (swatch_number)
	{
	case 1:
		draw8bitRect(7, 0, pixel_width - 2, pixel_width - 2, 0, 0); // clear out old swatch
		draw8bitRect(7, 0, pixel_width - 5, pixel_width - 5, 0, 1); // draw smaller version
		break;

	case 2:
		draw8bitRect(8, 0, pixel_width - 2, pixel_width - 2, 0, 0); // clear out old swatch
		draw8bitRect(8, 0, pixel_width - 5, pixel_width - 5, 0, 2); // draw smaller version
		break;

	case 3:
		draw8bitRect(9, 0, pixel_width - 2, pixel_width - 2, 0, 0); // clear out old swatch
		draw8bitRect(9, 0, pixel_width - 5, pixel_width - 5, 0, 3); // draw smaller version
		break;

	case 4:
		draw8bitRect(10, 0, pixel_width - 2, pixel_width - 2, 0, 0); // clear out old swatch
		draw8bitRect(10, 0, pixel_width - 5, pixel_width - 5, 0, 4); // draw smaller version
		break;

	case 5:
		draw8bitRect(11, 0, pixel_width - 2, pixel_width - 2, 0, 0); // clear out old swatch
		draw8bitRect(11, 0, pixel_width - 5, pixel_width - 5, 0, 5); // draw smaller version
		break;

	case 6:
		draw8bitRect(12, 0, pixel_width - 2, pixel_width - 2, 0, 0); // clear out old swatch
		draw8bitRect(12, 0, pixel_width - 5, pixel_width - 5, 0, 6); // draw smaller version
		break;
	}
}

void draw_screen_ca_mode()
{
	PA_8bitText(0, 3, 3, 255, 40, "sequencer control - press START to toggle on/off", 1, 1, 0, 100);
}

void draw_screen_set_clock_source_mode()
{
	if (sequencer_status == MANUAL)
	{
		draw_manual_tap_control();
	}
	else
	{
		draw_bpm();
	}
}

void draw_bpm()
{
	char buffer[10];

	// PA_8bitText (u8 screen, s16 basex, s16 basey, s16 maxx, s16 maxy, char *text, u8 color, u8 size, u8 transp, s32 limit)
	// example: PA_8bitText(0, 10, 20, 255, 40, text, 1, 3, 0, 100);

	PA_Clear8bitBg(0);
	PA_8bitText(0, 3, 3, 255, 40, "clock control:BPM - press Y to switch to STRUM", 1, 1, 0, 100);
	sprintf(buffer, "%d BPM", bpm);
	PA_8bitText(0, 90, 80, 255, 40, buffer, 1, 4, 0, 130);
	PA_8bitText(0, 30, 105, 255, 40, "Use up/down keypad for +/- 1 BPM", 1, 2, 0, 130);
	PA_8bitText(0, 25, 120, 255, 40, "Use right/left keypad for +/- 10 BPM", 1, 2, 0, 130);
	PA_8bitText(0, 65, 180, 255, 40, "BPM is not 100% accurate", 1, 1, 0, 130);
}

void draw_manual_tap_control()
{
	PA_Clear8bitBg(0);
	PA_8bitText(0, 3, 3, 255, 40, "clock control:STRUM - press Y to switch to BPM", 1, 1, 0, 100);
	draw8bitRectEx(0, 95, 255, 2, 0, 2); // x,y,w,h,screen,color
	PA_8bitText(0, 60, 180, 255, 40, "Press left keypad to rewind", 1, 1, 0, 130);
}

// Draw the entire freq_mod page

void draw_freq_mod_faders()
{

	int i;

	PA_Clear8bitBg(0);
	PA_8bitText(0, 3, 3, 255, 40, "frequency modulation", 1, 1, 0, 100);
	PA_8bitText(0, 45, 140, 255, 40, "Press Y to reset modulation", 1, 2, 0, 130);
	draw_swatches();
	choose_swatch(selected_swatch);

	for (i = 0; i < 32; i++)
	{
		draw8bitRectEx(i * 8, FREQ_MOD_FADER_HEIGHT - freq_mod[selected_swatch - 1][i], 7, freq_mod[selected_swatch - 1][i], 0, selected_swatch);
	}
}

void draw_freq_mod_fader(int i)
{
	draw8bitRectEx(i * 8, 40, 7, 100, 0, 0);
	draw8bitRectEx(i * 8, FREQ_MOD_FADER_HEIGHT - freq_mod[selected_swatch - 1][i], 7, freq_mod[selected_swatch - 1][i], 0, selected_swatch);
}

void draw_freq_mod_position()
{
	int old_sequencer_step = sequencer_step - 1;
	if (old_sequencer_step == -1)
		old_sequencer_step = 31;

	draw8bitRectEx(old_sequencer_step * 8, FREQ_MOD_FADER_HEIGHT + 2, 7, 7, 0, 0);			 // clear out old position indicator
	draw8bitRectEx(sequencer_step * 8, FREQ_MOD_FADER_HEIGHT + 2, 7, 7, 0, selected_swatch); // draw little position indicator
}

void draw_sequencer_step_position()
{
	int old_sequencer_step = sequencer_step - 1;
	if (old_sequencer_step == -1)
		old_sequencer_step = 31;

	draw8bitRectEx(old_sequencer_step * 8, 183, 7, 7, 1, 0); // clear out old position indicator
	draw8bitRectEx(sequencer_step * 8, 183, 7, 7, 1, 1);	 // draw little position indicator
}

void draw_screen_trigger_mode()
{
	PA_Clear8bitBg(0);
	PA_8bitText(0, 3, 3, 255, 40, "trigger configuration", 1, 1, 0, 100);
	PA_8bitText(0, 65, 180, 255, 40, "Press Y to reset triggers", 1, 1, 0, 130);
	draw_matrix(triggers, 0);
	draw_swatches();
	choose_swatch(selected_swatch);
}

void update_screen_pallet_mode()
{
	int i;
	int y_pos = 0;

	PA_Clear8bitBg(0);
	PA_8bitText(0, 3, 3, 255, 40, "choose sounds", 1, 1, 0, 100);
	PA_8bitText(0, 35, 180, 255, 40, "Press up/down to select sound, A to load", 1, 1, 0, 130);
	draw_swatches();
	choose_swatch(selected_swatch);

	for (i = file_offset; i < (file_offset + 9); i++)
	{
		if ((y_pos < 4) && (i >= 0) && (i <= number_of_files))
		{
			// PA_8bitText (u8 screen, s16 basex, s16 basey, s16 maxx, s16 maxy, char *text, u8 color, u8 size, u8 transp, s32 limit)
			PA_8bitText(0, 3, (y_pos * 12) + 35, 255, 40, filenames[i], 1, 0, 0, 100);
		}

		if (y_pos == 4)
		{
			PA_8bitText(0, 3, 90, 255, 40, filenames[i], 1, 3, 0, 100);
		}

		if ((y_pos > 4) && (i >= 0) && (i <= number_of_files))
		{
			// PA_8bitText (u8 screen, s16 basex, s16 basey, s16 maxx, s16 maxy, char *text, u8 color, u8 size, u8 transp, s32 limit)
			PA_8bitText(0, 3, (y_pos * 12) + 55, 255, 40, filenames[i], 1, 0, 0, 100);
		}

		y_pos++;
	}
}

void draw_screen_pallet_mode()
{
	int i;
	int y_pos = 0;

	PA_Clear8bitBg(0);
	PA_8bitText(0, 3, 3, 255, 40, "choose sounds", 1, 1, 0, 100);
	PA_8bitText(0, 35, 180, 255, 40, "Press up/down to select sound, A to load", 1, 1, 0, 130);
	draw_swatches();
	choose_swatch(selected_swatch);

	if (selected_swatch == 1)
		file_offset = find_position_of_filename_in_directory(swatch_0_file_assignment) - 4;
	if (selected_swatch == 2)
		file_offset = find_position_of_filename_in_directory(swatch_1_file_assignment) - 4;
	if (selected_swatch == 3)
		file_offset = find_position_of_filename_in_directory(swatch_2_file_assignment) - 4;
	if (selected_swatch == 4)
		file_offset = find_position_of_filename_in_directory(swatch_3_file_assignment) - 4;
	if (selected_swatch == 5)
		file_offset = find_position_of_filename_in_directory(swatch_4_file_assignment) - 4;
	if (selected_swatch == 6)
		file_offset = find_position_of_filename_in_directory(swatch_5_file_assignment) - 4;

	for (i = file_offset; i < (file_offset + 9); i++)
	{
		if ((y_pos < 4) && (i >= 0) && (i <= number_of_files))
		{
			// PA_8bitText (u8 screen, s16 basex, s16 basey, s16 maxx, s16 maxy, char *text, u8 color, u8 size, u8 transp, s32 limit)
			PA_8bitText(0, 3, (y_pos * 12) + 35, 255, 40, filenames[i], 1, 0, 0, 100);
		}

		if (y_pos == 4)
		{
			PA_8bitText(0, 3, 90, 255, 40, filenames[i], 1, 3, 0, 100);
		}

		if ((y_pos > 4) && (i >= 0) && (i <= number_of_files))
		{
			// PA_8bitText (u8 screen, s16 basex, s16 basey, s16 maxx, s16 maxy, char *text, u8 color, u8 size, u8 transp, s32 limit)
			PA_8bitText(0, 3, (y_pos * 12) + 55, 255, 40, filenames[i], 1, 0, 0, 100);
		}

		y_pos++;
	}
}

void get_sound_file_directory_listing()
{
	int file_number = 0;
	struct stat st;
	char filename[256]; // to hold a full filename and string terminator

	DIR_ITER *dir = diropen("/glitchDS/sounds");

	if (dir == NULL)
	{
		PA_OutputText(0, 2, 2, "Unable to open the directory.");
		PA_OutputText(0, 2, 3, "Please make sure that /glitchDS/sounds exists");
	}
	else
	{
		dirnext(dir, filename, &st); // eat up .
		dirnext(dir, filename, &st); // eat up ..

		while (dirnext(dir, filename, &st) == 0)
		{
			filenames[file_number] = (char *)malloc(strlen(filename) + 1);
			strcpy(filenames[file_number], filename);
			file_number++;
		}
	}

	number_of_files = file_number - 1;
}

void draw_screen_save_load_mode()
{
	char buffer[50];

	// PA_8bitText (u8 screen, s16 basex, s16 basey, s16 maxx, s16 maxy, char *text, u8 color, u8 size, u8 transp, s32 limit)
	// example: PA_8bitText(0, 10, 20, 255, 40, text, 1, 3, 0, 100);

	sprintf(buffer, "Slot %d", file_id);

	PA_Clear8bitBg(0);
	PA_8bitText(0, 3, 3, 255, 40, "save/load", 1, 1, 0, 100);

	PA_8bitText(0, 40, 80, 255, 40, buffer, 1, 4, 0, 130);

	PA_8bitText(0, 50, 105, 255, 40, "Press A to save", 1, 2, 0, 130);
	PA_8bitText(0, 50, 120, 255, 40, "Press B to load", 1, 2, 0, 130);

	PA_8bitText(0, 55, 180, 255, 40, "Press up/down to select slot", 1, 1, 0, 130);
}

void draw_screen_distortion_mode()
{
	PA_Clear8bitBg(0);
	PA_8bitText(0, 3, 3, 255, 40, "distortion settings", 1, 1, 0, 100);
	if ((distortion_y > 20) && (distortion_y < 185))
		draw8bitRectEx(0, distortion_y, 255, 2, 0, 2);
}

void init_freq_mod_array()
{
	int i, j;

	for (j = 0; j < 6; j++)
	{
		for (i = 0; i < 32; i++)
		{
			freq_mod[j][i] = FREQ_MOD_FADER_CENTER; // FREQ_MOD_FADER_CENTER is considered the "0" point, which is arbitrary
		}
	}
}

void reset_freq_mod()
{
	int i;

	for (i = 0; i < 32; i++)
	{
		freq_mod[selected_swatch - 1][i] = FREQ_MOD_FADER_CENTER; // FREQ_MOD_FADER_CENTER is considered the "0" point, which is arbitrary
	}
}

void next_state()
{
	sequencer_step++;

	if (sequencer_step == 32)
	{
		sequencer_step = 0;

		// reset automaton
		seq_play_audio(now, seed);
		swap_ca(now, seed);
	}
	else
	{
		clear_ca(next);
		calculate_ca(now, next);
		seq_play_audio(now, next);
		swap_ca(now, next);
	}
}

void stop_sequencer()
{
	sequencer_step = 0;
	sequencer_status = STOPPED;
	PA_VBLCounterPause(0);
	PA_VBLCounterPause(1);
}

void start_sequencer()
{
	sequencer_status = CLOCKED;
	seq_play_audio(now, seed);
	swap_ca(now, seed);
	PA_VBLCounterStart(1);
}

void do_interrupt()
{
	if (sequencer_status == CLOCKED)
		redraw_flag = 1;
}

int main(int argc, char **argv)
{

	bpm = 90;
	char filename[30];
	char large_buffer[100];
	int key_hold_time = 0;
	int i;

	// PALib Initialization
	PA_Init();
	PA_InitVBL();

	PA_SetBgPalCol(0, 1, PA_RGB(31, 31, 31));
	PA_SetBgPalCol(0, 2, PA_RGB(0, 0, 25));
	PA_SetBgPalCol(0, 3, PA_RGB(30, 5, 10));
	PA_SetBgPalCol(0, 4, PA_RGB(15, 0, 31));
	PA_SetBgPalCol(0, 5, PA_RGB(0, 15, 15));

	PA_SetBgPalCol(0, 6, PA_RGB(15, 15, 0));

	PA_SetBgPalCol(1, 1, PA_RGB(31, 31, 31));

	PA_Init8bitBg(0, 3);
	PA_Init8bitBg(1, 3);

	PA_InitText(1, 0);
	PA_InitText(0, 0);

	PA_SetTextCol(1, 31, 31, 31);
	PA_SetTextCol(0, 0, 0, 31);

	int channel;
	// int slot_id;
	int stylus_x = 0;
	int stylus_y = 0;

	PA_8bitText(0, 3, 3, 255, 40, "Loading...", 1, 1, 0, 100); // PA_OutputText(bool screen, u16 x, u16 y, const char *text, arguments...)

	PA_WaitForVBL();
	PA_WaitForVBL();
	PA_WaitForVBL();  // wait a few VBLs
	fatInitDefault(); // Initialise fat library

	// Init AS_Lib for normal sound playback only
	// PA_InitASLibForSounds(AS_MODE_SURROUND | AS_MODE_16CH);
	// AS_SetDefaultSettings(AS_PCM_16BIT, 22050, AS_NO_DELAY);
	PA_InitSound();

	get_sound_file_directory_listing();

	initialize_sounds();

	clear_ca(now);
	clear_ca(seed);
	clear_triggers(triggers);

	int old_stylus_x = 10;
	int old_stylus_y = 10;
	int old_bpm = 0;
	int drew_freq_mod = 0;

	int mode = DRAW_CA;

	// 15 x 11

	triggers[7][7] = 1;
	triggers[7][9] = 1;
	triggers[4][4] = 1;
	triggers[8][8] = 2;
	triggers[3][3] = 2;
	triggers[8][7] = 4;
	triggers[6][6] = 5;
	triggers[3][3] = 6;
	triggers[6][4] = 4;
	triggers[5][6] = 3;
	triggers[7][8] = 3;

	// Set interrupt

	TIMER_CR(1) = TIMER_ENABLE | TIMER_DIV_1024 | TIMER_IRQ_REQ;
	TIMER_DATA(1) = (int)(-(0x2000000 >> 10) / (float)((bpm * 4.0) / (float)60));
	irqSet(IRQ_TIMER1, do_interrupt);

	// Initialize frequency mod

	init_freq_mod_array();

	// PA_8bitText (u8 screen, s16 basex, s16 basey, s16 maxx, s16 maxy, char *text, u8 color, u8 size, u8 transp, s32 limit)
	// example: PA_8bitText(0, 10, 20, 255, 40, text, 1, 3, 0, 100);

	PA_Clear8bitBg(0);
	PA_Clear8bitBg(1);

	draw_screen_ca_mode();

	// Infinite loop to keep the program
	while (1)
	{

		// :: Switch Modes

		if (Pad.Newpress.R)
		{
			mode++;
			if (mode > NUMBER_OF_MODES)
				mode = 1;
		}

		if (Pad.Newpress.L)
		{
			mode--;
			if (mode < 1)
				mode = NUMBER_OF_MODES;
		}

		if (Pad.Newpress.L || Pad.Newpress.R)
		{
			PA_Clear8bitBg(0);
			// PA_Clear8bitBg(1);

			switch (mode)
			{
			case DRAW_CA:
				draw_ca(seed, 0);
				draw_screen_ca_mode(); // was draw_sequencer_control
				break;

			case SET_TRIGGERS:
				draw_screen_trigger_mode();
				break;

			case PALLET_MODE:
				draw_screen_pallet_mode();
				break;

			case SET_CLOCK_SOURCE:
				draw_screen_set_clock_source_mode();
				break;

			case FREQ_MOD_MODE:
				draw_freq_mod_faders();
				draw_freq_mod_position();
				break;

			case DISTORTION_MODE:
				draw_screen_distortion_mode();
				break;

			case SAVE_LOAD_MODE:
				draw_screen_save_load_mode();
				break;
			}
		}

		// :: Mode Actions ::

		if (mode == DRAW_CA)
		{
			if (Pad.Newpress.Y)
			{
				PA_Clear8bitBg(0);
				clear_ca(seed);
				draw_ca(seed, 0);
				draw_screen_ca_mode(); // was draw_sequencer_control
			}

			if (Stylus.Held)
			{
				stylus_x = Stylus.X / pixel_width;
				stylus_y = Stylus.Y / pixel_width;

				if ((stylus_x > 0) && (stylus_x < GRID_WIDTH) && (stylus_y > 0) && (stylus_y < GRID_HEIGHT))
				{

					if ((stylus_x != old_stylus_x) || (stylus_y != old_stylus_y) || Stylus.Newpress)
					{
						if (seed[stylus_x][stylus_y] == 0)
						{
							seed[stylus_x][stylus_y] = 1;
							draw8bitRect(stylus_x, stylus_y, pixel_width, pixel_width, 0, 1); // int x, int y, int w, int h, bool screen, int color
						}
						else
						{
							seed[stylus_x][stylus_y] = 0;
							draw8bitRect(stylus_x, stylus_y, pixel_width, pixel_width, 0, 0); // int x, int y, int w, int h, bool screen, int color
						}

						old_stylus_x = stylus_x;
						old_stylus_y = stylus_y;
					}
				}
			}
			if (redraw_flag)
				draw_ca(now, 1);
		}

		// User is setting trigger points

		if (mode == SET_TRIGGERS)
		{
			// Handle the case where the user sets or erases a trigger

			stylus_x = Stylus.X / pixel_width;
			stylus_y = Stylus.Y / pixel_width;

			if ((stylus_x > 0) && (stylus_x < GRID_WIDTH) && (stylus_y > 0) && (stylus_y < GRID_HEIGHT))
			{
				if (Stylus.Newpress)
				{
					if (triggers[stylus_x][stylus_y] == 0)
					{
						triggers[stylus_x][stylus_y] = selected_swatch;									// change to use selected color
						draw8bitRect(stylus_x, stylus_y, pixel_width, pixel_width, 0, selected_swatch); // int x, int y, int w, int h, bool screen, int color
					}
					else
					{
						triggers[stylus_x][stylus_y] = 0;
						draw8bitRect(stylus_x, stylus_y, pixel_width, pixel_width, 0, 0); // int x, int y, int w, int h, bool screen, int color
					}

					old_stylus_x = stylus_x;
					old_stylus_y = stylus_y;
				}
			}

			// Handle reset of all triggers
			if (Pad.Newpress.Y)
			{
				clear_triggers(triggers);
				draw_screen_trigger_mode();
			}

			// Handle selection of swatches
			poll_swatches();

			// Continue to draw the sequencer state on the top screen
			if (redraw_flag)
				draw_ca(now, 1);
		}

		if (mode == SET_CLOCK_SOURCE)
		{

			if (Pad.Newpress.Y)
			{
				if (sequencer_status != MANUAL)
				{
					stop_sequencer();
					sequencer_status = MANUAL;
					draw_sequencer_step_position();
				}
				else
				{
					start_sequencer();
				}
				draw_screen_set_clock_source_mode();
			}

			if (sequencer_status == CLOCKED)
			{
				old_bpm = bpm;

				if (Pad.Newpress.Up)
					if (bpm < 300)
						bpm += 1;
				if (Pad.Newpress.Down)
					if (bpm > 49)
						bpm -= 1;
				if (Pad.Newpress.Right)
					if (bpm < 291)
						bpm += 10;
				if (Pad.Newpress.Left)
					if (bpm > 60)
						bpm -= 10;

				if (old_bpm != bpm)
				{
					old_bpm = bpm;
					TIMER_DATA(1) = (int)(-(0x2000000 >> 10) / ((bpm * 4) / (float)60));
					draw_screen_set_clock_source_mode();
				}
			}

			// Handle manual clock control

			if (sequencer_status == MANUAL)
			{

				stylus_x = Stylus.X;
				stylus_y = Stylus.Y;

				if (Pad.Newpress.Left)
				{
					if (stylus_y < 96)
						strum_quadrant = STRUM_TOP_HALF;
					if (stylus_y >= 96)
						strum_quadrant = STRUM_BOTTOM_HALF;
					sequencer_step = 0;
					clear_ca(now); // just added
					seq_play_audio(now, seed);
					swap_ca(now, seed);
					draw_sequencer_step_position();
				}

				if (Pad.Newpress.Right)
				{
					draw_ca(now, 1);
					next_state();
					draw_sequencer_step_position();
				}

				if ((strum_quadrant == STRUM_TOP_HALF) && (stylus_y >= 96))
				{
					strum_quadrant = STRUM_BOTTOM_HALF;
					draw_ca(now, 1);
					next_state();
					draw_sequencer_step_position();
				}

				if ((strum_quadrant == STRUM_BOTTOM_HALF) && (stylus_y < 96))
				{
					strum_quadrant = STRUM_TOP_HALF;
					draw_ca(now, 1);
					next_state();
					draw_sequencer_step_position();
				}
			}

			// Continue to draw the sequencer state on the top screen
			if (redraw_flag)
				draw_ca(now, 1);
		}

		if (mode == FREQ_MOD_MODE)
		{
			drew_freq_mod = 0;

			// Allow user to change mod faders using the stylus
			if (Stylus.Held)
			{
				stylus_x = Stylus.X / 8;
				stylus_y = Stylus.Y;

				if ((stylus_y > 40) && (stylus_y < FREQ_MOD_FADER_HEIGHT))
				{
					freq_mod[selected_swatch - 1][stylus_x] = FREQ_MOD_FADER_HEIGHT - stylus_y;
					draw_freq_mod_fader(stylus_x);
					draw_freq_mod_position();
					drew_freq_mod = 1;
				}
			}

			// Handle frequency reset

			if (Pad.Newpress.Y)
			{
				reset_freq_mod();
				draw_freq_mod_faders();
				draw_freq_mod_position();
			}

			// Handle selection of swatches
			if (poll_swatches())
			{
				if (!drew_freq_mod)
				{
					draw_freq_mod_faders();
					draw_freq_mod_position();
				}
				drew_freq_mod = 1;
			}

			// Continue to draw the sequencer state on the top screen
			if (redraw_flag)
			{
				draw_ca(now, 1);
				draw_freq_mod_position();
			}
		}

		//
		// --- DISTORTION_MODE ---
		//

		if (mode == DISTORTION_MODE)
		{

			stylus_x = Stylus.X;
			stylus_y = Stylus.Y;

			// Allow user to change mod faders using the stylus
			if (Stylus.Held)
			{
				if (stylus_y > 20)
				{
					if (old_stylus_y != stylus_y)
					{
						// clear out old vertical line
						if (old_stylus_y > 20)
							draw8bitRectEx(0, old_stylus_y, 255, 2, 0, 0); // x,y,w,h,screen,color

						if (stylus_y > 185)
						{
							distortion = 0;
							distortion_y = 0;
						}
						else
						{
							distortion = (185 - stylus_y) * DISTORTION_RANGE;
							draw8bitRectEx(0, stylus_y, 255, 2, 0, 2); // x,y,w,h,screen,color
							distortion_y = stylus_y;
						}

						old_stylus_x = stylus_x;
						old_stylus_y = stylus_y;
					}
				}
			}

			// Continue to draw the sequencer state on the top screen
			if (redraw_flag)
				draw_ca(now, 1);
		}

		//
		// --- PALLET_MODE ---
		//

		if (mode == PALLET_MODE)
		{

			if (Pad.Newpress.Down)
			{
				if ((file_offset + 4) < number_of_files)
					file_offset++;
				update_screen_pallet_mode();
			}

			if (Pad.Newpress.Up)
			{
				if ((file_offset + 4) > 0)
					file_offset--;
				update_screen_pallet_mode();
			}

			if (!Pad.Held.Down && !Pad.Held.Up)
				key_hold_time = 0;

			if (Pad.Held.Down)
			{
				key_hold_time++;

				if ((key_hold_time > 10) && (key_hold_time % 3 == 0)) // the modulus slows down the scroll
				{
					if ((file_offset + 4) < number_of_files)
					{
						file_offset++;
						update_screen_pallet_mode();
					}
				}
			}

			if (Pad.Held.Up)
			{
				key_hold_time++;

				if ((key_hold_time > 10) && (key_hold_time % 3 == 0)) // the modulus slows down the scroll
				{
					if ((file_offset + 4) > 0)
					{
						file_offset--;
						update_screen_pallet_mode();
					}
				}
			}

			if (Pad.Newpress.A)
			{
				load_sound_from_file(selected_swatch - 1, filenames[file_offset + 4]);
				AS_SoundPlay(sound_slot[selected_swatch - 1]);
			}

			if (poll_swatches())
			{
				draw_screen_pallet_mode();
			}

			// Continue to draw the sequencer state on the top screen
			if (redraw_flag)
				draw_ca(now, 1);
		}

		//
		// --- SAVE LOAD MODE ---
		//

		if (mode == SAVE_LOAD_MODE)
		{
			// Pad up/down select sound
			if (Pad.Newpress.Up || Pad.Newpress.Down)
			{

				if (Pad.Newpress.Up)
					file_id++;
				if (Pad.Newpress.Down)
					file_id--;

				if (file_id > 19)
					file_id = 0;
				if (file_id < 0)
					file_id = 19;

				draw_screen_save_load_mode();
			}

			// Continue to draw the sequencer state on the top screen
			if (redraw_flag)
				draw_ca(now, 1);

			if (Pad.Newpress.A)
			{

				sprintf(filename, "/glitchDS/snapshots/glitchDS.save.%d", file_id);

				FILE *fp = fopen(filename, "wb");

				if (fp != NULL)
				{
					fwrite((void *)seed, sizeof(seed), 1, fp);
					fwrite((void *)triggers, sizeof(triggers), 1, fp);
					fwrite((void *)sound_slot, sizeof(sound_slot), 1, fp);
					fwrite((void *)freq_mod, sizeof(freq_mod), 1, fp);
					fwrite((void *)channel_frequencies, sizeof(channel_frequencies), 1, fp);
					fwrite(&bpm, sizeof(bpm), 1, fp);
					fwrite(&distortion, sizeof(distortion), 1, fp);
					fwrite(&distortion_level, sizeof(distortion_level), 1, fp);
					fwrite(&distortion_y, sizeof(distortion_y), 1, fp);
					fwrite(swatch_0_file_assignment, sizeof(swatch_0_file_assignment), 1, fp);
					fwrite(swatch_1_file_assignment, sizeof(swatch_1_file_assignment), 1, fp);
					fwrite(swatch_2_file_assignment, sizeof(swatch_2_file_assignment), 1, fp);
					fwrite(swatch_3_file_assignment, sizeof(swatch_3_file_assignment), 1, fp);
					fwrite(swatch_4_file_assignment, sizeof(swatch_4_file_assignment), 1, fp);
					fwrite(swatch_5_file_assignment, sizeof(swatch_5_file_assignment), 1, fp);
					fclose(fp);

					sprintf(large_buffer, "Saved glitchDS.save.%d", file_id);

					PA_Clear8bitBg(0);
					PA_8bitText(0, 20, 80, 255, 40, large_buffer, 1, 3, 0, 130);
				}
				else
				{
					PA_Clear8bitBg(0);
					PA_8bitText(0, 0, 80, 255, 40, "Error creating save file.", 1, 1, 0, 130);
				}
			}

			if (Pad.Newpress.B)
			{

				sprintf(filename, "/glitchDS/snapshots/glitchDS.save.%d", file_id);

				FILE *fp = fopen(filename, "rb"); // rb = read

				// clear out and free up all sound stored in memory
				for (i = 0; i < 6; i++)
				{
					free(sound_slot[i].data);
					sound_slot[i].data = NULL;
				}

				if (fp != NULL)
				{
					stop_sequencer();
					fread((void *)seed, sizeof(seed), 1, fp);
					fread((void *)triggers, sizeof(triggers), 1, fp);
					fread((void *)sound_slot, sizeof(sound_slot), 1, fp);
					fread((void *)freq_mod, sizeof(freq_mod), 1, fp);
					fread((void *)channel_frequencies, sizeof(channel_frequencies), 1, fp);
					fread(&bpm, sizeof(bpm), 1, fp);
					fread(&distortion, sizeof(distortion), 1, fp);
					fread(&distortion_level, sizeof(distortion_level), 1, fp);
					fread(&distortion_y, sizeof(distortion_y), 1, fp);
					fread(swatch_0_file_assignment, sizeof(swatch_0_file_assignment), 1, fp);
					fread(swatch_1_file_assignment, sizeof(swatch_1_file_assignment), 1, fp);
					fread(swatch_2_file_assignment, sizeof(swatch_2_file_assignment), 1, fp);
					fread(swatch_3_file_assignment, sizeof(swatch_3_file_assignment), 1, fp);
					fread(swatch_4_file_assignment, sizeof(swatch_4_file_assignment), 1, fp);
					fread(swatch_5_file_assignment, sizeof(swatch_5_file_assignment), 1, fp);
					fclose(fp);

					// When the sound_slot structure is loaded, its .data is pointing somewhere random.
					// Clean it out.

					for (i = 0; i < 6; i++)
					{
						sound_slot[i].data = NULL;
					}

					load_sound_from_file(0, swatch_0_file_assignment);
					load_sound_from_file(1, swatch_1_file_assignment);
					load_sound_from_file(2, swatch_2_file_assignment);
					load_sound_from_file(3, swatch_3_file_assignment);
					load_sound_from_file(4, swatch_4_file_assignment);
					load_sound_from_file(5, swatch_5_file_assignment);

					start_sequencer();

					sprintf(large_buffer, "Loaded glitchDS.save.%d", file_id);

					PA_Clear8bitBg(0);
					PA_8bitText(0, 20, 80, 255, 40, large_buffer, 1, 3, 0, 130);
				}
				else
				{
					sprintf(large_buffer, "Saved file glitchDS.save.%d does not exist.", file_id);
					PA_Clear8bitBg(0);
					PA_8bitText(0, 0, 80, 255, 40, large_buffer, 1, 1, 0, 130);
				}
			}

			if (Pad.Newpress.Left || Pad.Newpress.Right)
			{
				draw_screen_save_load_mode();
			}
		}

		// :: Handle START/STOP

		if (Pad.Newpress.Start)
		{
			if (sequencer_status != CLOCKED)
			{
				start_sequencer();
			}
			else
			{
				stop_sequencer();
			}
		}

		if (redraw_flag)
		{
			next_state();
			redraw_flag = 0;
		}

		// Do distortion calculations

		if (distortion != 0)
		{
			if (distortion_level < 0)
			{
				distortion_level = distortion;
			}
			else
			{
				distortion_level = -1 * distortion;
			}

			for (channel = 0; channel < 8; channel++)
			{
				AS_SetSoundRate(channel, channel_frequencies[channel] + distortion_level);
			}
		}

		PA_WaitForVBL();
	}

	return 0;
}

void load_sound_from_file(int slot_id, char *filename)
{
	FILE *fp;
	u32 buffer_size;
	char *buffer;
	char *full_filename;
	char *path = "/glitchDS/sounds/";
	u8 *old_buffer;
	char tmp_filename[100];

	strcpy(tmp_filename, filename); // just in case

	full_filename = (char *)calloc(strlen(filename) + strlen(path) + 1, sizeof(char));

	strcat(full_filename, path);
	strcat(full_filename, filename);

	fp = fopen(full_filename, "rb");

	if (fp == NULL)
	{
		PA_Clear8bitBg(1);
		PA_OutputSimpleText(1, 1, 5, "unable to read file");
		PA_OutputSimpleText(1, 1, 6, full_filename);
		while (1)
		{
			PA_WaitForVBL();
		}
	}

	// PA_OutputSimpleText(1, 1, 6, full_filename);

	fseek(fp, 0, SEEK_END);
	buffer_size = ftell(fp);
	rewind(fp);

	buffer = (char *)malloc(sizeof(char) * buffer_size);
	fread(buffer, buffer_size, 1, fp);
	fclose(fp);

	old_buffer = sound_slot[slot_id].data;

	sound_slot[slot_id].data = (u8 *)buffer;
	sound_slot[slot_id].size = (u32)buffer_size;
	sound_slot[slot_id].format = (u32)1;
	sound_slot[slot_id].volume = (u8)128;
	sound_slot[slot_id].rate = (s32)22050;
	sound_slot[slot_id].loop = (u8)0;

	if (old_buffer != NULL)
		free(old_buffer); // free up old memory
	free(full_filename);

	if (slot_id == 0)
	{
		swatch_0_file_assignment[0] = '\0';
		strcpy(swatch_0_file_assignment, tmp_filename);
	}
	if (slot_id == 1)
	{
		swatch_1_file_assignment[0] = '\0';
		strcpy(swatch_1_file_assignment, tmp_filename);
	}
	if (slot_id == 2)
	{
		swatch_2_file_assignment[0] = '\0';
		strcpy(swatch_2_file_assignment, tmp_filename);
	}
	if (slot_id == 3)
	{
		swatch_3_file_assignment[0] = '\0';
		strcpy(swatch_3_file_assignment, tmp_filename);
	}
	if (slot_id == 4)
	{
		swatch_4_file_assignment[0] = '\0';
		strcpy(swatch_4_file_assignment, tmp_filename);
	}
	if (slot_id == 5)
	{
		swatch_5_file_assignment[0] = '\0';
		strcpy(swatch_5_file_assignment, tmp_filename);
	}
}

int find_position_of_filename_in_directory(char *filename)
{
	int i;

	for (i = 0; i < number_of_files + 1; i++)
	{
		if (strcmp(filename, filenames[i]) == 0)
		{
			return (i);
		}
	}

	PA_Clear8bitBg(1);
	PA_OutputSimpleText(1, 1, 5, "unable to find");
	PA_OutputSimpleText(1, 1, 6, filename);
	while (1)
	{
		PA_WaitForVBL();
	}

	return -1;
}

void initialize_sounds()
{
	int i;

	for (i = 0; i < 6; i++)
	{
		load_sound_from_file(i, filenames[i]);
	}
}
