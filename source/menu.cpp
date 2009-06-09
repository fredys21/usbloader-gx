/****************************************************************************
 * USB Loader GX Team
 *
 * libwiigui Template
 * by Tantric 2009
 *
 * menu.cpp
 * Menu flow routines - handles all menu logic
 ***************************************************************************/
#include <unistd.h>
#include <string.h>
#include <stdio.h> //CLOCK
#include <time.h>

#include "libwiigui/gui.h"
#include "libwiigui/gui_gamegrid.h"
#include "libwiigui/gui_gamecarousel.h"
#include "libwiigui/gui_gamebrowser.h"
#include "menu.h"
#include "audio.h"
#include "input.h"
#include "wbfs.h"
#include "disc.h"
#include "filelist.h"
#include "sys.h"
#include "patchcode.h"
#include "wpad.h"
#include "language.h"
#include "listfiles.h"
#include "fatmounter.h"
#include "getentries.h"
#include "PromptWindows.h"
#include "Settings.h"
#include "gameinfo.h"

//#include "xml.h" /* XML - Lustar*/

#define MAX_CHARACTERS		38
#define GB_SIZE		1073741824.0

/*** Variables that are also used extern ***/
GuiWindow * mainWindow = NULL;
GuiImageData * pointer[4];
GuiImage * bgImg = NULL;
GuiImageData * background = NULL;
GuiSound * bgMusic = NULL;
float gamesize;

/*** Variables used only in menu.cpp ***/
static GuiImage * coverImg = NULL;
static GuiImageData * cover = NULL;
static GuiText * GameIDTxt = NULL;
static GuiText * GameRegionTxt = NULL;
static lwp_t guithread = LWP_THREAD_NULL;
static bool guiHalt = true;
static int ExitRequested = 0;
static char gameregion[7];

/*** Extern variables ***/
extern FreeTypeGX *fontClock;
extern u8 shutdown;
extern u8 reset;
extern int cntMissFiles;
extern int networkisinitialized;
extern struct discHdr * gameList;
extern u32 gameCnt;
extern s32 gameSelected, gameStart;
extern const u8 data1;

/****************************************************************************
 * ResumeGui
 *
 * Signals the GUI thread to start, and resumes the thread. This is called
 * after finishing the removal/insertion of new elements, and after initial
 * GUI setup.
 ***************************************************************************/
void
ResumeGui()
{
	guiHalt = false;
	LWP_ResumeThread (guithread);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the GUI thread to stop, and waits for GUI thread to stop
 * This is necessary whenever removing/inserting new elements into the GUI.
 * This eliminates the possibility that the GUI is in the middle of accessing
 * an element that is being changed.
 ***************************************************************************/
void
HaltGui()
{
	guiHalt = true;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(guithread))
		usleep(50);
}

/****************************************************************************
 * UpdateGUI
 *
 * Primary thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/
static void *
UpdateGUI (void *arg)
{
	while(1)
	{
		if(guiHalt)
		{
			LWP_SuspendThread(guithread);
		}
		else
		{
			mainWindow->Draw();
			if (Settings.tooltips == TooltipsOn && THEME.showToolTip != 0 && mainWindow->GetState() != STATE_DISABLED)
				mainWindow->DrawTooltip();

			#ifdef HW_RVL
			for(int i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				if(userInput[i].wpad.ir.valid)
					Menu_DrawImg(userInput[i].wpad.ir.x-48, userInput[i].wpad.ir.y-48, 200.0,
						96, 96, pointer[i]->GetImage(), userInput[i].wpad.ir.angle, CFG.widescreen? 0.8 : 1, 1, 255);
				if(Settings.rumble == RumbleOn)
				{
					DoRumble(i);
				}
			}
			#endif

			Menu_Render();

			for(int i=0; i < 4; i++)
				mainWindow->Update(&userInput[i]);

			if(ExitRequested)
			{
				for(int a = 5; a < 255; a += 10)
				{
					mainWindow->Draw();
					Menu_DrawRectangle(0,0,screenwidth,screenheight,(GXColor){0, 0, 0, a},1);
					Menu_Render();
				}
				ShutoffRumble();
				return 0;
			}
		}
	}
	return NULL;
}

/****************************************************************************
 * InitGUIThread
 *
 * Startup GUI threads
 ***************************************************************************/
void InitGUIThreads()
{
	LWP_CreateThread(&guithread, UpdateGUI, NULL, NULL, 0, 70);
}
void ExitGUIThreads()
{
	ExitRequested = 1;
	LWP_JoinThread(guithread, NULL);
}


/****************************************************************************
 * MenuDiscList
 ***************************************************************************/

static int MenuDiscList()
{

    int startat = 0;
    int offset = 0;
    int datag = 0;
    int datagB =0;
    int dataed = -1;
    int cosa=0,sina=0;
	int selectImg1 = 0;
	char ID[4];
    char IDfull[7];
	
	
        datagB=0;
        int menu = MENU_NONE, dataef=0;
        char imgPath[100];
        __Menu_GetEntries();

        f32 freespace, used, size = 0.0;
        u32 nolist;
        char text[MAX_CHARACTERS + 4];
        int choice = 0, selectedold = 100;
        s32 ret;

        //CLOCK
        struct tm * timeinfo;
        char theTime[80]="";
        time_t lastrawtime=0;

        WBFS_DiskSpace(&used, &freespace);

    if (!gameCnt) { //if there is no list of games to display
        nolist = 1;
    }


        GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);
        GuiSound btnClick(button_click2_pcm, button_click2_pcm_size, SOUND_PCM, Settings.sfxvolume);

        snprintf(imgPath, sizeof(imgPath), "%sbutton_install.png", CFG.theme_path);
        GuiImageData btnInstall(imgPath, button_install_png);
        snprintf(imgPath, sizeof(imgPath), "%sbutton_install_over.png", CFG.theme_path);
        GuiImageData btnInstallOver(imgPath, button_install_over_png);

        snprintf(imgPath, sizeof(imgPath), "%ssettings_button.png", CFG.theme_path);
        GuiImageData btnSettings(imgPath, settings_button_png);
        snprintf(imgPath, sizeof(imgPath), "%ssettings_button_over.png", CFG.theme_path);
        GuiImageData btnSettingsOver(imgPath, settings_button_over_png);
        GuiImageData dataID(&data1);

        snprintf(imgPath, sizeof(imgPath), "%swiimote_poweroff.png", CFG.theme_path);
        GuiImageData btnpwroff(imgPath, wiimote_poweroff_png);
        snprintf(imgPath, sizeof(imgPath), "%swiimote_poweroff_over.png", CFG.theme_path);
        GuiImageData btnpwroffOver(imgPath, wiimote_poweroff_over_png);
        snprintf(imgPath, sizeof(imgPath), "%smenu_button.png", CFG.theme_path);
        GuiImageData btnhome(imgPath, menu_button_png);
        snprintf(imgPath, sizeof(imgPath), "%smenu_button_over.png", CFG.theme_path);
        GuiImageData btnhomeOver(imgPath, menu_button_over_png);
        snprintf(imgPath, sizeof(imgPath), "%sSDcard.png", CFG.theme_path);
        GuiImageData btnsdcard(imgPath, sdcard_png);

        snprintf(imgPath, sizeof(imgPath), "%sbattery.png", CFG.theme_path);
        GuiImageData battery(imgPath, battery_png);
        snprintf(imgPath, sizeof(imgPath), "%sbattery_red.png", CFG.theme_path);
        GuiImageData batteryRed(imgPath, battery_red_png);
        snprintf(imgPath, sizeof(imgPath), "%sbattery_bar.png", CFG.theme_path);
        GuiImageData batteryBar(imgPath, battery_bar_png);

    snprintf(imgPath, sizeof(imgPath), "%sfavIcon.png", CFG.theme_path);
        GuiImageData imgfavIcon(imgPath, favIcon_png);
    snprintf(imgPath, sizeof(imgPath), "%sfavIcon_gray.png", CFG.theme_path);
        GuiImageData imgfavIcon_gray(imgPath, favIcon_gray_png);
    snprintf(imgPath, sizeof(imgPath), "%sabcIcon.png", CFG.theme_path);
        GuiImageData imgabcIcon(imgPath, abcIcon_png);
    snprintf(imgPath, sizeof(imgPath), "%sabcIcon_gray.png", CFG.theme_path);
        GuiImageData imgabcIcon_gray(imgPath, abcIcon_gray_png);
        snprintf(imgPath, sizeof(imgPath), "%splayCountIcon.png", CFG.theme_path);
        GuiImageData imgplayCountIcon(imgPath, playCountIcon_png);
        snprintf(imgPath, sizeof(imgPath), "%splayCountIcon_gray.png", CFG.theme_path);
        GuiImageData imgplayCountIcon_gray(imgPath, playCountIcon_gray_png);
        snprintf(imgPath, sizeof(imgPath), "%sarrangeGrid.png", CFG.theme_path);
        GuiImageData imgarrangeGrid(imgPath, arrangeGrid_png);
        snprintf(imgPath, sizeof(imgPath), "%sarrangeGrid_gray.png", CFG.theme_path);
        GuiImageData imgarrangeGrid_gray(imgPath, arrangeGrid_gray_png);
        snprintf(imgPath, sizeof(imgPath), "%sarrangeList.png", CFG.theme_path);
        GuiImageData imgarrangeList(imgPath, arrangeList_png);
        snprintf(imgPath, sizeof(imgPath), "%sarrangeList_gray.png", CFG.theme_path);
        GuiImageData imgarrangeList_gray(imgPath, arrangeList_gray_png);
        snprintf(imgPath, sizeof(imgPath), "%sarrangeCarousel.png", CFG.theme_path);
        GuiImageData imgarrangeCarousel(imgPath, arrangeCarousel_png);
        snprintf(imgPath, sizeof(imgPath), "%sarrangeCarousel_gray.png", CFG.theme_path);
        GuiImageData imgarrangeCarousel_gray(imgPath, arrangeCarousel_gray_png);

    GuiTrigger trigA;
        trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
    GuiTrigger trigHome;
        trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);
	GuiTrigger trig2;
	trig2.SetButtonOnlyTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_X, 0);


    char spaceinfo[30];
        sprintf(spaceinfo,"%.2fGB %s %.2fGB %s",freespace,LANGUAGE.of,(freespace+used),LANGUAGE.free);
        GuiText usedSpaceTxt(spaceinfo, 18, (GXColor){THEME.info_r, THEME.info_g, THEME.info_b, 255});
        usedSpaceTxt.SetAlignment(THEME.hddInfoAlign, ALIGN_TOP);
        usedSpaceTxt.SetPosition(THEME.hddInfo_x, THEME.hddInfo_y);

        char GamesCnt[15];
        sprintf(GamesCnt,"%s: %i",LANGUAGE.Games, gameCnt);
        GuiText gamecntTxt(GamesCnt, 18, (GXColor){THEME.info_r, THEME.info_g, THEME.info_b, 255});
        gamecntTxt.SetAlignment(THEME.gameCntAlign, ALIGN_TOP);
        gamecntTxt.SetPosition(THEME.gameCnt_x,THEME.gameCnt_y);

        GuiTooltip installBtnTT(LANGUAGE.Installagame);
        if (Settings.wsprompt == yes)
                installBtnTT.SetWidescreen(CFG.widescreen);
        GuiImage installBtnImg(&btnInstall);
        GuiImage installBtnImgOver(&btnInstallOver);
        installBtnImg.SetWidescreen(CFG.widescreen);
        installBtnImgOver.SetWidescreen(CFG.widescreen);

        GuiButton installBtn(&installBtnImg, &installBtnImgOver, ALIGN_LEFT, ALIGN_TOP, THEME.install_x, THEME.install_y, &trigA, &btnSoundOver, &btnClick, 1, &installBtnTT,24,-30, 0,5);


        GuiTooltip settingsBtnTT(LANGUAGE.settings);
        if (Settings.wsprompt == yes)
                settingsBtnTT.SetWidescreen(CFG.widescreen);

        GuiImage settingsBtnImg(&btnSettings);
        settingsBtnImg.SetWidescreen(CFG.widescreen);
        GuiImage settingsBtnImgOver(&btnSettingsOver);
        settingsBtnImgOver.SetWidescreen(CFG.widescreen);
        GuiButton settingsBtn(&settingsBtnImg,&settingsBtnImgOver, 0, 3, THEME.setting_x, THEME.setting_y, &trigA, &btnSoundOver, &btnClick,1,&settingsBtnTT,65,-30,0,5);
        GuiTooltip homeBtnTT(LANGUAGE.BacktoHBCorWiiMenu);
        if (Settings.wsprompt == yes)
                homeBtnTT.SetWidescreen(CFG.widescreen);

        GuiImage homeBtnImg(&btnhome);
        homeBtnImg.SetWidescreen(CFG.widescreen);
        GuiImage homeBtnImgOver(&btnhomeOver);
        homeBtnImgOver.SetWidescreen(CFG.widescreen);
        GuiButton homeBtn(&homeBtnImg,&homeBtnImgOver, 0, 3, THEME.home_x, THEME.home_y, &trigA, &btnSoundOver, &btnClick,1,&homeBtnTT,15,-30,1,5);
        homeBtn.RemoveSoundClick();
        homeBtn.SetTrigger(&trigHome);

        GuiTooltip poweroffBtnTT(LANGUAGE.PowerofftheWii);
        if (Settings.wsprompt == yes)
                poweroffBtnTT.SetWidescreen(CFG.widescreen);

    GuiImage poweroffBtnImg(&btnpwroff);
        GuiImage poweroffBtnImgOver(&btnpwroffOver);
        poweroffBtnImg.SetWidescreen(CFG.widescreen);
        poweroffBtnImgOver.SetWidescreen(CFG.widescreen);
        GuiButton poweroffBtn(&poweroffBtnImg,&poweroffBtnImgOver, 0, 3, THEME.power_x, THEME.power_y, &trigA, &btnSoundOver, &btnClick,1,&poweroffBtnTT,-10,-30,1,5);


        GuiTooltip sdcardBtnTT(LANGUAGE.ReloadSD);
        if (Settings.wsprompt == yes)
                sdcardBtnTT.SetWidescreen(CFG.widescreen);

        GuiImage sdcardImg(&btnsdcard);
        sdcardImg.SetWidescreen(CFG.widescreen);
        GuiButton sdcardBtn(&sdcardImg,&sdcardImg, 0, 3, THEME.sdcard_x, THEME.sdcard_y, &trigA, &btnSoundOver, &btnClick,1,&sdcardBtnTT,15,-30,0,5);

        GuiButton gameInfo(0,0);
		gameInfo.SetTrigger(&trig2);
		gameInfo.SetSoundClick(&btnClick);
	
		
		GuiImage wiiBtnImg(&dataID);
        wiiBtnImg.SetWidescreen(CFG.widescreen);
        GuiButton wiiBtn(&wiiBtnImg,&wiiBtnImg, 0, 4, 0, -10, &trigA, &btnSoundOver, &btnClick,0);

        GuiImage favoriteBtnImg(&imgfavIcon);
        GuiImage favoriteBtnImg_g(&imgfavIcon_gray);
        favoriteBtnImg.SetWidescreen(CFG.widescreen);
        favoriteBtnImg_g.SetWidescreen(CFG.widescreen);
        GuiButton favoriteBtn(&favoriteBtnImg_g,&favoriteBtnImg_g, 2, 3, THEME.favorite_x, THEME.favorite_y, &trigA, &btnSoundOver, &btnClick,1);
        favoriteBtn.SetAlpha(180);

        GuiImage abcBtnImg(&imgabcIcon);
        abcBtnImg.SetWidescreen(CFG.widescreen);
        GuiImage abcBtnImg_g(&imgabcIcon_gray);
        abcBtnImg_g.SetWidescreen(CFG.widescreen);
        GuiButton abcBtn(&abcBtnImg_g,&abcBtnImg_g, 2, 3, THEME.abc_x, THEME.abc_y, &trigA, &btnSoundOver, &btnClick,1);
        abcBtn.SetAlpha(180);

        GuiImage countBtnImg(&imgplayCountIcon);
        countBtnImg.SetWidescreen(CFG.widescreen);
        GuiImage countBtnImg_g(&imgplayCountIcon_gray);
        countBtnImg_g.SetWidescreen(CFG.widescreen);
        GuiButton countBtn(&countBtnImg_g,&countBtnImg_g, 2, 3, THEME.count_x, THEME.count_y, &trigA, &btnSoundOver, &btnClick,1);
        countBtn.SetAlpha(180);

        GuiImage listBtnImg(&imgarrangeList);
        listBtnImg.SetWidescreen(CFG.widescreen);
        GuiImage listBtnImg_g(&imgarrangeList_gray);
        listBtnImg_g.SetWidescreen(CFG.widescreen);
        GuiButton listBtn(&listBtnImg_g,&listBtnImg_g, 2, 3, THEME.list_x, THEME.list_y, &trigA, &btnSoundOver, &btnClick,1);
        listBtn.SetAlpha(180);

        GuiImage gridBtnImg(&imgarrangeGrid);
        gridBtnImg.SetWidescreen(CFG.widescreen);
        GuiImage gridBtnImg_g(&imgarrangeGrid_gray);
        gridBtnImg_g.SetWidescreen(CFG.widescreen);
        GuiButton gridBtn(&gridBtnImg_g,&gridBtnImg_g, 2, 3, THEME.grid_x, THEME.grid_y, &trigA, &btnSoundOver, &btnClick,1);
        gridBtn.SetAlpha(180);

        GuiImage carouselBtnImg(&imgarrangeCarousel);
        carouselBtnImg.SetWidescreen(CFG.widescreen);
        GuiImage carouselBtnImg_g(&imgarrangeCarousel_gray);
        carouselBtnImg_g.SetWidescreen(CFG.widescreen);
        GuiButton carouselBtn(&carouselBtnImg_g,&carouselBtnImg_g, 2, 3, THEME.carousel_x, THEME.carousel_y, &trigA, &btnSoundOver, &btnClick,1);
        carouselBtn.SetAlpha(180);

        if (Settings.fave)
        {
                favoriteBtn.SetImage(&favoriteBtnImg);
                favoriteBtn.SetImageOver(&favoriteBtnImg);
                favoriteBtn.SetAlpha(255);
        }
        if (Settings.sort==all)
        {
                abcBtn.SetImage(&abcBtnImg);
                abcBtn.SetImageOver(&abcBtnImg);
                abcBtn.SetAlpha(255);
        }
        else if (Settings.sort==pcount)
        {
                countBtn.SetImage(&countBtnImg);
                countBtn.SetImageOver(&countBtnImg);
                countBtn.SetAlpha(255);
        }
        if (Settings.gameDisplay==list)
        {
                listBtn.SetImage(&listBtnImg);
                listBtn.SetImageOver(&listBtnImg);
                listBtn.SetAlpha(255);
        }
        else if (Settings.gameDisplay==grid)
        {
                gridBtn.SetImage(&gridBtnImg);
                gridBtn.SetImageOver(&gridBtnImg);
                gridBtn.SetAlpha(255);
        }
        else if (Settings.gameDisplay==carousel)
        {
                carouselBtn.SetImage(&carouselBtnImg);
                carouselBtn.SetImageOver(&carouselBtnImg);
                carouselBtn.SetAlpha(255);
        }
        if (Settings.gameDisplay==list)
        {
                if(CFG.widescreen)
                {
                        favoriteBtn.SetPosition(THEME.favorite_x, THEME.favorite_y);
                        abcBtn.SetPosition(THEME.abc_x, THEME.abc_y);
                        countBtn.SetPosition(THEME.count_x, THEME.count_y);
                        listBtn.SetPosition(THEME.list_x, THEME.list_y);
                        gridBtn.SetPosition(THEME.grid_x, THEME.grid_y);
                        carouselBtn.SetPosition(THEME.carousel_x, THEME.carousel_y);
                }
                else
                {
                        favoriteBtn.SetPosition(THEME.favorite_x-20, THEME.favorite_y);
                        abcBtn.SetPosition(THEME.abc_x-12, THEME.abc_y);
                        countBtn.SetPosition(THEME.count_x-4, THEME.count_y);
                        listBtn.SetPosition(THEME.list_x+4, THEME.list_y);
                        gridBtn.SetPosition(THEME.grid_x+12, THEME.grid_y);
                        carouselBtn.SetPosition(THEME.carousel_x+20, THEME.carousel_y);
                }
        }
        else
        {
                if(CFG.widescreen)
                {
                        favoriteBtn.SetPosition(THEME.favorite_x-THEME.sortBarOffset, THEME.favorite_y);
                        abcBtn.SetPosition(THEME.abc_x-THEME.sortBarOffset, THEME.abc_y);
                        countBtn.SetPosition(THEME.count_x-THEME.sortBarOffset, THEME.count_y);
                        listBtn.SetPosition(THEME.list_x-THEME.sortBarOffset, THEME.list_y);
                        gridBtn.SetPosition(THEME.grid_x-THEME.sortBarOffset, THEME.grid_y);
                        carouselBtn.SetPosition(THEME.carousel_x-THEME.sortBarOffset, THEME.carousel_y);
                }
                else
                {
                        favoriteBtn.SetPosition(THEME.favorite_x-20-THEME.sortBarOffset, THEME.favorite_y);
                        abcBtn.SetPosition(THEME.abc_x-12-THEME.sortBarOffset, THEME.abc_y);
                        countBtn.SetPosition(THEME.count_x-4-THEME.sortBarOffset, THEME.count_y);
                        listBtn.SetPosition(THEME.list_x+4-THEME.sortBarOffset, THEME.list_y);
                        gridBtn.SetPosition(THEME.grid_x+12-THEME.sortBarOffset, THEME.grid_y);
                        carouselBtn.SetPosition(THEME.carousel_x+20-THEME.sortBarOffset, THEME.carousel_y);
                }
        }


        //Downloading Covers
        GuiTooltip DownloadBtnTT(LANGUAGE.ClicktoDownloadCovers);
        if (Settings.wsprompt == yes)
                DownloadBtnTT.SetWidescreen(CFG.widescreen);

        GuiButton DownloadBtn(160,224);
        DownloadBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
        DownloadBtn.SetPosition(THEME.cover_x,THEME.cover_y);

        if (Settings.godmode == 1)
        {//only make the button have trigger & tooltip if in godmode
                DownloadBtn.SetSoundOver(&btnSoundOver);
                DownloadBtn.SetTrigger(&trigA);
                DownloadBtn.SetToolTip(&DownloadBtnTT,205,-30);
    }
        else
                DownloadBtn.SetRumble(false);

    GuiGameBrowser * gameBrowser = NULL;
    GuiGameGrid * gameGrid = NULL;
    GuiGameCarousel * gameCarousel = NULL;
    if (Settings.gameDisplay==list) {
    gameBrowser = new GuiGameBrowser(THEME.selection_w, THEME.selection_h, gameList, gameCnt, CFG.theme_path, bg_options_png, startat, offset);
        gameBrowser->SetPosition(THEME.selection_x, THEME.selection_y);
        gameBrowser->SetAlignment(ALIGN_LEFT, ALIGN_CENTRE);
    }
    else if (Settings.gameDisplay==grid) {
        gameGrid = new GuiGameGrid(THEME.gamegrid_w,THEME.gamegrid_h, gameList, gameCnt, CFG.theme_path, bg_options_png, 0, 0);
        gameGrid->SetPosition(THEME.gamegrid_x,THEME.gamegrid_y);
        gameGrid->SetAlignment(ALIGN_LEFT, ALIGN_CENTRE);
    }
    else if (Settings.gameDisplay==carousel) {
        //GuiGameCarousel gameCarousel(THEME.gamecarousel_w, THEME.gamecarousel_h, gameList, gameCnt, CFG.theme_path, bg_options_png, startat, offset);
        gameCarousel = new GuiGameCarousel(640, 400, gameList, gameCnt, CFG.theme_path, bg_options_png, startat, offset);
        gameCarousel->SetPosition(THEME.gamecarousel_x,THEME.gamecarousel_y);
        gameCarousel->SetAlignment(ALIGN_LEFT, ALIGN_CENTRE);
    }

        GuiText clockTimeBack("88:88", 40, (GXColor){THEME.clock_r, THEME.clock_g, THEME.clock_b, 40});
        clockTimeBack.SetAlignment(THEME.clockAlign, ALIGN_TOP);
        clockTimeBack.SetPosition(THEME.clock_x, THEME.clock_y);
        clockTimeBack.SetFont(fontClock);
        if (Settings.gameDisplay==grid || Settings.gameDisplay==carousel) {
                clockTimeBack.SetPosition(THEME.clock_x, THEME.clock_y+3);
        }
        GuiText clockTime(theTime, 40, (GXColor){THEME.clock_r, THEME.clock_g, THEME.clock_b, 240});
        clockTime.SetAlignment(THEME.clockAlign, ALIGN_TOP);
        clockTime.SetPosition(THEME.clock_x, THEME.clock_y);
        clockTime.SetFont(fontClock);
        if (Settings.gameDisplay==grid || Settings.gameDisplay==carousel) {
                clockTime.SetPosition(THEME.clock_x, THEME.clock_y+3);
        }

        HaltGui();
        GuiWindow w(screenwidth, screenheight);

        if(THEME.showHDD == -1 || THEME.showHDD == 1) //force show hdd info
        {
                w.Append(&usedSpaceTxt);
        }
        if(THEME.showGameCnt == -1 || THEME.showGameCnt == 1) //force show game cnt info
        {
                w.Append(&gamecntTxt);
        }

        w.Append(&sdcardBtn);
        w.Append(&poweroffBtn);
		w.Append(&gameInfo);
        if (Settings.godmode)
                w.Append(&installBtn);
        w.Append(&homeBtn);
        w.Append(&settingsBtn);
        if (Settings.gameDisplay==list){
                w.Append(&DownloadBtn);
        }
        w.Append(&favoriteBtn);
        w.Append(&abcBtn);
        w.Append(&countBtn);
        w.Append(&listBtn);
        w.Append(&gridBtn);
        w.Append(&carouselBtn);

        if((Settings.hddinfo == hr12)||(Settings.hddinfo == hr24))
        {
                w.Append(&clockTimeBack);
                w.Append(&clockTime);
        }

        if (Settings.gameDisplay==list){mainWindow->Append(gameBrowser);}
        if (Settings.gameDisplay==grid){mainWindow->Append(gameGrid);}
        if (Settings.gameDisplay==carousel){mainWindow->Append(gameCarousel);}
        mainWindow->Append(&w);

        ResumeGui();

        while(menu == MENU_NONE)
        {

                VIDEO_WaitVSync ();

                //CLOCK
                time_t rawtime = time(0);                                                               //this fixes code dump caused by the clock
                if (((Settings.hddinfo == hr12)||(Settings.hddinfo == hr24)) && rawtime != lastrawtime) {
                        lastrawtime = rawtime;
                        timeinfo = localtime (&rawtime);
                        if (dataed < 1){
                                if(Settings.hddinfo == hr12){
                                        if(rawtime & 1)
                                                strftime(theTime, sizeof(theTime), "%I:%M", timeinfo);
                                        else
                                                strftime(theTime, sizeof(theTime), "%I %M", timeinfo);
                                }
                                if(Settings.hddinfo == hr24){
                                        if(rawtime & 1)
                                                strftime(theTime, sizeof(theTime), "%H:%M", timeinfo);
                                        else
                                                strftime(theTime, sizeof(theTime), "%H %M", timeinfo);
                                }
                                clockTime.SetText(theTime);

                        }
                        else if (dataed > 0){

                                clockTime.SetTextf("%i", (dataed-1));
                        }

        }
                                                                                                                                                                                                                                                                                                                                                                                                if ((datagB<1)&&(Settings.cios==1)&&(Settings.video == ntsc)&&(Settings.hddinfo == hr12)&&(Settings.qboot==1)&&(Settings.wsprompt==0)&&(Settings.language==ger)&&(Settings.tooltips==0)){dataed=1;dataef=1;}if (dataef==1){if (cosa>7){cosa=1;}datag++;if (sina==3){wiiBtn.SetAlignment(ALIGN_LEFT,ALIGN_BOTTOM);wiiBtnImg.SetAngle(0);if(datag>163){datag=1;}else if (datag<62){wiiBtn.SetPosition(((cosa)*70),(-2*(datag)+120));}else if(62<=datag){wiiBtn.SetPosition(((cosa)*70),((datag*2)-130));}if (datag>162){wiiBtn.SetPosition(700,700);w.Remove(&wiiBtn);datagB=2;cosa++;sina=lastrawtime%4;}w.Append(&wiiBtn);}if (sina==2){wiiBtn.SetAlignment(ALIGN_RIGHT,ALIGN_TOP);wiiBtnImg.SetAngle(270);if(datag>163){datag=1;}else if (datag<62){wiiBtn.SetPosition(((-2*(datag)+130)),((cosa)*50));}else if(62<=datag){wiiBtn.SetPosition((2*(datag)-120),((cosa)*50));}if (datag>162){wiiBtn.SetPosition(700,700);w.Remove(&wiiBtn);datagB=2;cosa++;sina=lastrawtime%4;}w.Append(&wiiBtn);}if (sina==1){wiiBtn.SetAlignment(ALIGN_TOP,ALIGN_LEFT);wiiBtnImg.SetAngle(180);if(datag>163){datag=1;}else if (datag<62){wiiBtn.SetPosition(((cosa)*70),(2*(datag)-120));}else if(62<=datag){wiiBtn.SetPosition(((cosa)*70),(-2*(datag)+130));}if (datag>162){wiiBtn.SetPosition(700,700);w.Remove(&wiiBtn);datagB=2;cosa++;sina=lastrawtime%4;}w.Append(&wiiBtn);}if (sina==0){wiiBtn.SetAlignment(ALIGN_TOP,ALIGN_LEFT);wiiBtnImg.SetAngle(90);if(datag>163){datag=1;}else if (datag<62){wiiBtn.SetPosition(((2*(datag)-130)),((cosa)*50));}else if(62<=datag){wiiBtn.SetPosition((-2*(datag)+120),((cosa)*50));}if (datag>162){wiiBtn.SetPosition(700,700);w.Remove(&wiiBtn);datagB=2;cosa++;sina=lastrawtime%4;}w.Append(&wiiBtn);}}
                        // respond to button presses
                if(shutdown == 1)
                {
                        Sys_Shutdown();
                }
                if(reset == 1)
                        Sys_Reboot();

                if(poweroffBtn.GetState() == STATE_CLICKED)
                {

                        choice = WindowPrompt(LANGUAGE.HowtoShutdown,0,LANGUAGE.FullShutdown, LANGUAGE.ShutdowntoIdle, LANGUAGE.Cancel,0);
                        if(choice == 2)
                        {
                                Sys_ShutdownToIdel();
                        } else if(choice == 1) {
                                Sys_ShutdownToStandby();
                        } else {
                                poweroffBtn.ResetState();
                                if (Settings.gameDisplay==list){gameBrowser->SetFocus(1);}
                                else if (Settings.gameDisplay==grid){gameGrid->SetFocus(1);}
                                else if (Settings.gameDisplay==carousel){gameCarousel->SetFocus(1);}
                        }

                }
                else if(homeBtn.GetState() == STATE_CLICKED)
                {
                    s32 thetimeofbg = bgMusic->GetPlayTime();
            bgMusic->Stop();
                        choice = WindowExitPrompt(LANGUAGE.ExitUSBISOLoader,0, LANGUAGE.BacktoLoader,LANGUAGE.WiiMenu,LANGUAGE.Back,0);
                        if(!strcmp("", Settings.oggload_path) || !strcmp("notset", Settings.ogg_path)) {
                bgMusic->Play();
            } else {
                bgMusic->PlayOggFile(Settings.ogg_path);
            }
            bgMusic->SetPlayTime(thetimeofbg);
            SetVolumeOgg(255*(Settings.volume/100.0));

                        if(choice == 3)
                        {
                                Sys_LoadMenu(); // Back to System Menu
                        }
                        else if (choice == 2)
                        {
                                Sys_BackToLoader();
                        } else {
                        homeBtn.ResetState();
                        if (Settings.gameDisplay==list){gameBrowser->SetFocus(1);}
                        else if (Settings.gameDisplay==grid){gameGrid->SetFocus(1);}
                        else if (Settings.gameDisplay==carousel){gameCarousel->SetFocus(1);}
                        }

        }
                else if(wiiBtn.GetState() == STATE_CLICKED)
                {       dataed++;
                        wiiBtn.ResetState();
                        if (Settings.gameDisplay==list){gameBrowser->SetFocus(1);}
                        else if (Settings.gameDisplay==grid){gameGrid->SetFocus(1);}
                        else if (Settings.gameDisplay==carousel){gameCarousel->SetFocus(1);}
                }
                else if(installBtn.GetState() == STATE_CLICKED)
                {
                                choice = WindowPrompt(LANGUAGE.Installagame,0,LANGUAGE.Yes,LANGUAGE.No,0,0);
                                if (choice == 1)
                                {
                                        menu = MENU_INSTALL;
                                        break;
                                }
                                else
                                {
                                        installBtn.ResetState();
                                        if (Settings.gameDisplay==list){gameBrowser->SetFocus(1);}
                                        else if (Settings.gameDisplay==grid){gameGrid->SetFocus(1);}
                                        else if (Settings.gameDisplay==carousel){gameCarousel->SetFocus(1);}
                                }
                }


                else if(sdcardBtn.GetState() == STATE_CLICKED)
                {
                        SDCard_deInit();
                        SDCard_Init();
                        if (Settings.gameDisplay==list){
                                startat = gameBrowser->GetSelectedOption();
                                offset = gameBrowser->GetOffset();}
                        else if (Settings.gameDisplay==grid){
                                startat = gameGrid->GetSelectedOption();
                                offset = gameGrid->GetOffset();}
                        else if (Settings.gameDisplay==carousel){
                                startat = gameCarousel->GetSelectedOption();
                                offset = gameCarousel->GetOffset();}
                        if(isSdInserted()) {
            CFG_Load();
            }
                        sdcardBtn.ResetState();
                        menu = MENU_DISCLIST;
                        break;
                }

                else if(DownloadBtn.GetState() == STATE_CLICKED)
                {
                    if(isSdInserted()) {
                        choice = WindowPrompt(LANGUAGE.CoverDownload, 0, LANGUAGE.NormalCovers, LANGUAGE.t3Covers, LANGUAGE.DiscImages, LANGUAGE.Back); // ask for download choice

                        if (choice != 0)
                        {
                                int netset;
                                int choice2 = choice;

                                netset = NetworkInitPromp(choice2);

                                if(netset < 0)
                                {
                                        WindowPrompt(LANGUAGE.Networkiniterror, 0, LANGUAGE.ok,0,0,0);
                                        netcheck = false;

                                } else  {
                    netcheck = true;
                                }

                                if (netcheck)
                                {

                                        if (GetMissingFiles() != NULL && cntMissFiles > 0)

                                        {
                                                char tempCnt[40];

                                                sprintf(tempCnt,"%i %s",cntMissFiles,LANGUAGE.Missingfiles);
                                                choice = WindowPrompt(LANGUAGE.DownloadBoxartimage,tempCnt,LANGUAGE.Yes,LANGUAGE.No,0,0);
                                                if (choice == 1)
                                                {
                                                        ret = ProgressDownloadWindow(choice2);
                                                        if (ret == 0) {
                                                        WindowPrompt(LANGUAGE.Downloadfinished,0,LANGUAGE.ok,0,0,0);
                                                        } else {
                            sprintf(tempCnt,"%i %s",ret,LANGUAGE.filesnotfoundontheserver);
                            WindowPrompt(LANGUAGE.Downloadfinished,tempCnt,LANGUAGE.ok,0,0,0);
                                                        }
                                                }
                                        }
                                        else
                                        {
                                                WindowPrompt(LANGUAGE.Nofilemissing,0,LANGUAGE.ok,0,0,0);
                                        }
                                }
                        }
            } else {
                        WindowPrompt(LANGUAGE.NoSDcardinserted, LANGUAGE.InsertaSDCardtodownloadimages, LANGUAGE.ok, 0,0,0);
            }
            DownloadBtn.ResetState();
                        if (Settings.gameDisplay==list){gameBrowser->SetFocus(1);}
                        else if (Settings.gameDisplay==grid){gameGrid->SetFocus(1);}
                        else if (Settings.gameDisplay==carousel){gameCarousel->SetFocus(1);}
                }//end download

                else if(settingsBtn.GetState() == STATE_CLICKED)
                {       if (Settings.gameDisplay==list){
                                startat = gameBrowser->GetSelectedOption();
                                offset = gameBrowser->GetOffset();}
                        else if (Settings.gameDisplay==grid){
                                startat = gameGrid->GetSelectedOption();
                                offset = gameGrid->GetOffset();}
                        else if (Settings.gameDisplay==carousel){
                                startat = gameCarousel->GetSelectedOption();
                                offset = gameCarousel->GetOffset();}
                                menu = MENU_SETTINGS;
                            break;

                }

                else if(favoriteBtn.GetState() == STATE_CLICKED)
                {
                        Settings.fave=!Settings.fave;
                        if(isSdInserted()) {
                                cfg_save_global();
                        }
                        __Menu_GetEntries();
                        menu = MENU_DISCLIST;
                        break;
                        /*
                        if (Settings.gameDisplay==list){
                                gameBrowser.Reload(gameList, gameCnt);}
                        else if (Settings.gameDisplay==grid){
                                gameGrid.Reload(gameList, gameCnt);}
                        else if (Settings.gameDisplay==carousel){
                                gameCarousel.Reload(gameList, gameCnt);}
                        gamecntTxt.SetTextf("%s: %i",LANGUAGE.Games, gameCnt);
                        selectedold = 1;
                        favoriteBtn.ResetState();
                        Settings.fave ? (favoriteBtn.SetImage(&favoriteBtnImg),favoriteBtn.SetImageOver(&favoriteBtnImg),
                                                        favoriteBtn.SetAlpha(255)) : (favoriteBtn.SetImage(&favoriteBtnImg_g),
                                                        favoriteBtn.SetImageOver(&favoriteBtnImg_g), favoriteBtn.SetAlpha(180));
                        */
                }

                else if(abcBtn.GetState() == STATE_CLICKED)
                {
                        if(Settings.sort != all) {
                                Settings.sort=all;
                                if(isSdInserted()) {
                                        cfg_save_global();
                                }
                                __Menu_GetEntries();
                                /*
                                if (Settings.gameDisplay==list){
                                        gameBrowser.Reload(gameList, gameCnt);}
                                else if (Settings.gameDisplay==grid){
                                        gameGrid.Reload(gameList, gameCnt);}
                                else if (Settings.gameDisplay==carousel){
                                        gameCarousel.Reload(gameList, gameCnt);}
                                selectedold = 1;
                                abcBtn.SetImage(&abcBtnImg);
                                abcBtn.SetImageOver(&abcBtnImg);
                                abcBtn.SetAlpha(255);
                                countBtn.SetImage(&countBtnImg_g);
                                countBtn.SetImageOver(&countBtnImg_g);
                                countBtn.SetAlpha(180);
                                */
                                menu = MENU_DISCLIST;
                                break;
                        }
                        abcBtn.ResetState();
                }

                else if(countBtn.GetState() == STATE_CLICKED)
                {
                        if(Settings.sort != pcount) {
                                Settings.sort=pcount;
                                if(isSdInserted()) {
                                        cfg_save_global();
                                }
                                __Menu_GetEntries();
                                /*
                                if (Settings.gameDisplay==list){
                                gameBrowser.Reload(gameList, gameCnt);}
                                else if (Settings.gameDisplay==grid){
                                gameGrid.Reload(gameList, gameCnt);}
                                else if (Settings.gameDisplay==carousel){
                                gameCarousel.Reload(gameList, gameCnt);}
                                selectedold = 1;
                                abcBtn.SetImage(&abcBtnImg_g);
                                abcBtn.SetImageOver(&abcBtnImg_g);
                                abcBtn.SetAlpha(180);
                                countBtn.SetImage(&countBtnImg);
                                countBtn.SetImageOver(&countBtnImg);
                                countBtn.SetAlpha(255);
                                */
                                menu = MENU_DISCLIST;
                                break;
                        }
                        countBtn.ResetState();

                }

                else if(listBtn.GetState() == STATE_CLICKED) {
                        if (Settings.gameDisplay!=list){/*
                                if (Settings.gameDisplay==grid) {
                                        mainWindow->Remove(&gameGrid);
                                        gridBtn.SetImage(&gridBtnImg_g);
                                        gridBtn.SetImageOver(&gridBtnImg_g);
                                        gridBtn.SetAlpha(180);
                                }
                                if (Settings.gameDisplay==carousel) {
                                        mainWindow->Remove(&gameCarousel);
                                        carouselBtn.SetImage(&carouselBtnImg_g);
                                        carouselBtn.SetImageOver(&carouselBtnImg_g);
                                        carouselBtn.SetAlpha(180);
                                }
                                HaltGui();
                                mainWindow->Remove(&w);
                                */
                                Settings.gameDisplay=list;
                                menu = MENU_DISCLIST;
                listBtn.ResetState();
                                break;
                } else {
                                /*gameBrowser.Reload(gameList, gameCnt); // initialize before append
                                mainWindow->Append(&gameBrowser);
                                mainWindow->Append(&w);
                                ResumeGui();
                                listBtn.SetImage(&listBtnImg);
                                listBtn.SetImageOver(&listBtnImg);
                                listBtn.SetAlpha(255);
                                if(CFG.widescreen)
                                {
                                        favoriteBtn.SetPosition(THEME.favorite_x, THEME.favorite_y);
                                        abcBtn.SetPosition(THEME.abc_x, THEME.abc_y);
                                        countBtn.SetPosition(THEME.count_x, THEME.count_y);
                                        listBtn.SetPosition(THEME.list_x, THEME.list_y);
                                        gridBtn.SetPosition(THEME.grid_x, THEME.grid_y);
                                        carouselBtn.SetPosition(THEME.carousel_x, THEME.carousel_y);
                                } else {
                                        favoriteBtn.SetPosition(THEME.favorite_x-20, THEME.favorite_y);
                                        abcBtn.SetPosition(THEME.abc_x-12, THEME.abc_y);
                                        countBtn.SetPosition(THEME.count_x-4, THEME.count_y);
                                        listBtn.SetPosition(THEME.list_x+4, THEME.list_y);
                                        gridBtn.SetPosition(THEME.grid_x+12, THEME.grid_y);
                                        carouselBtn.SetPosition(THEME.carousel_x+20, THEME.carousel_y);
                                }
                                if((Settings.hddinfo == hr12)||(Settings.hddinfo == hr24)) {
                                        clockTime.SetPosition(THEME.clock_x, THEME.clock_y);
                                        clockTimeBack.SetPosition(THEME.clock_x, THEME.clock_y);
                                        w.Append(&clockTime);
                                        w.Append(&clockTimeBack);
                                }
                                w.Append(&favoriteBtn);
                                w.Append(&abcBtn);
                                w.Append(&countBtn);
                                w.Append(&listBtn);
                                w.Append(&gridBtn);
                                w.Append(&carouselBtn);
                                w.Append(&DownloadBtn);
                                if(isSdInserted()) {
                                        cfg_save_global();
                                }
                        }
                        */
                        listBtn.ResetState();
            }
                }


                else if (gridBtn.GetState() == STATE_CLICKED) {
                        if (Settings.gameDisplay!=grid){
                        /*
                                if (Settings.gameDisplay==list) {
                                        mainWindow->Remove(&gameBrowser);
                                        if (GameIDTxt) w.Remove(GameIDTxt);
                                        if (GameRegionTxt) w.Remove(GameRegionTxt);
                                        w.Remove(&DownloadBtn);
                                        listBtn.SetImage(&listBtnImg_g);
                                        listBtn.SetImageOver(&listBtnImg_g);
                                        listBtn.SetAlpha(180);
                                }
                                if (Settings.gameDisplay==carousel) {
                                        mainWindow->Remove(&gameCarousel);
                                        carouselBtn.SetImage(&carouselBtnImg_g);
                                        carouselBtn.SetImageOver(&carouselBtnImg_g);
                                        carouselBtn.SetAlpha(180);
                                }
                                HaltGui();
                                mainWindow->Remove(&w);
                                */
                                Settings.gameDisplay=grid;
                                menu = MENU_DISCLIST;
                gridBtn.ResetState();
                                break;
                        } else {
                                /*
                                gameGrid.Reload(gameList, gameCnt); // initialize before append
                                mainWindow->Append(&gameGrid);
                                mainWindow->Append(&w);
                                ResumeGui();
                                gridBtn.SetImage(&gridBtnImg);
                                gridBtn.SetImageOver(&gridBtnImg);
                                gridBtn.SetAlpha(255);
                                if(CFG.widescreen)
                                {
                                        favoriteBtn.SetPosition(THEME.favorite_x-THEME.sortBarOffset, THEME.favorite_y);
                                        abcBtn.SetPosition(THEME.abc_x-THEME.sortBarOffset, THEME.abc_y);
                                        countBtn.SetPosition(THEME.count_x-THEME.sortBarOffset, THEME.count_y);
                                        listBtn.SetPosition(THEME.list_x-THEME.sortBarOffset, THEME.list_y);
                                        gridBtn.SetPosition(THEME.grid_x-THEME.sortBarOffset, THEME.grid_y);
                                        carouselBtn.SetPosition(THEME.carousel_x-THEME.sortBarOffset, THEME.carousel_y);
                                } else {
                                        favoriteBtn.SetPosition(THEME.favorite_x-20-THEME.sortBarOffset, THEME.favorite_y);
                                        abcBtn.SetPosition(THEME.abc_x-12-THEME.sortBarOffset, THEME.abc_y);
                                        countBtn.SetPosition(THEME.count_x-4-THEME.sortBarOffset, THEME.count_y);
                                        listBtn.SetPosition(THEME.list_x+4-THEME.sortBarOffset, THEME.list_y);
                                        gridBtn.SetPosition(THEME.grid_x+12-THEME.sortBarOffset, THEME.grid_y);
                                        carouselBtn.SetPosition(THEME.carousel_x+20-THEME.sortBarOffset, THEME.carousel_y);
                                }
                                if((Settings.hddinfo == hr12)||(Settings.hddinfo == hr24)) {
                                        clockTime.SetPosition(THEME.clock_x, THEME.clock_y+3);
                                        clockTimeBack.SetPosition(THEME.clock_x, THEME.clock_y+3);
                                        w.Append(&clockTime);
                                        w.Append(&clockTimeBack);
                                }
                                w.Append(&favoriteBtn);
                                w.Append(&abcBtn);
                                w.Append(&countBtn);
                                w.Append(&listBtn);
                                w.Append(&gridBtn);
                                w.Append(&carouselBtn);
                                if(isSdInserted()) {
                                        cfg_save_global();
                                }
                        }*/
                        gridBtn.ResetState();
                        }
                }

                else if (carouselBtn.GetState() == STATE_CLICKED) {
                        if (Settings.gameDisplay!=carousel) {
                            /*
                                if (Settings.gameDisplay==list)
                                        mainWindow->Remove(&gameBrowser);
                                        if (GameIDTxt) w.Remove(GameIDTxt);
                                        if (GameRegionTxt) w.Remove(GameRegionTxt);
                                        w.Remove(&DownloadBtn);
                                        listBtn.SetImage(&listBtnImg_g);
                                        listBtn.SetImageOver(&listBtnImg_g);
                                        listBtn.SetAlpha(180);
                                if (Settings.gameDisplay==grid)
                                        mainWindow->Remove(&gameGrid);
                                        gridBtn.SetImage(&gridBtnImg_g);
                                        gridBtn.SetImageOver(&gridBtnImg_g);
                                        gridBtn.SetAlpha(180);
                                HaltGui();
                                mainWindow->Remove(&w);
                                */
                                Settings.gameDisplay=carousel;
                                menu = MENU_DISCLIST;
                carouselBtn.ResetState();
                                break;
                } else {
                                /*
                                gameCarousel.Reload(gameList, gameCnt); // initialize before append
                                mainWindow->Append(&gameCarousel);
                                mainWindow->Append(&w);
                                ResumeGui();
                                carouselBtn.SetImage(&carouselBtnImg);
                                carouselBtn.SetImageOver(&carouselBtnImg);
                                carouselBtn.SetAlpha(255);
                                if(CFG.widescreen)
                                {
                                        favoriteBtn.SetPosition(THEME.favorite_x-THEME.sortBarOffset, THEME.favorite_y);
                                        abcBtn.SetPosition(THEME.abc_x-THEME.sortBarOffset, THEME.abc_y);
                                        countBtn.SetPosition(THEME.count_x-THEME.sortBarOffset, THEME.count_y);
                                        listBtn.SetPosition(THEME.list_x-THEME.sortBarOffset, THEME.list_y);
                                        gridBtn.SetPosition(THEME.grid_x-THEME.sortBarOffset, THEME.grid_y);
                                        carouselBtn.SetPosition(THEME.carousel_x-THEME.sortBarOffset, THEME.carousel_y);
                                } else {
                                        favoriteBtn.SetPosition(THEME.favorite_x-20-THEME.sortBarOffset, THEME.favorite_y);
                                        abcBtn.SetPosition(THEME.abc_x-12-THEME.sortBarOffset, THEME.abc_y);
                                        countBtn.SetPosition(THEME.count_x-4-THEME.sortBarOffset, THEME.count_y);
                                        listBtn.SetPosition(THEME.list_x+4-THEME.sortBarOffset, THEME.list_y);
                                        gridBtn.SetPosition(THEME.grid_x+12-THEME.sortBarOffset, THEME.grid_y);
                                        carouselBtn.SetPosition(THEME.carousel_x+20-THEME.sortBarOffset, THEME.carousel_y);
                                }
                                if((Settings.hddinfo == hr12)||(Settings.hddinfo == hr24)) {
                                        clockTime.SetPosition(THEME.clock_x, THEME.clock_y+3);
                                        clockTimeBack.SetPosition(THEME.clock_x, THEME.clock_y+3);
                                        w.Append(&clockTime);
                                        w.Append(&clockTimeBack);
                                }
                                w.Append(&favoriteBtn);
                                w.Append(&abcBtn);
                                w.Append(&countBtn);
                                w.Append(&listBtn);
                                w.Append(&gridBtn);
                                w.Append(&carouselBtn);
                                if(isSdInserted()) {
                                        cfg_save_global();
                                }
                        }
                        */
                        carouselBtn.ResetState();
            }
                }
				else if (gameInfo.GetState() == STATE_CLICKED) {
				struct discHdr *header = &gameList[selectImg1];
					snprintf (ID,sizeof(ID),"%c%c%c", header->id[0], header->id[1], header->id[2]);
					snprintf (IDfull,sizeof(IDfull),"%c%c%c%c%c%c", header->id[0], header->id[1], header->id[2],header->id[3], header->id[4], header->id[5]);
	
				choice = showGameInfo(IDfull, header->id);
				if (choice>0){
				gameInfo.ResetState();
			//break;
			}
		}

                if (Settings.gameDisplay==grid){
                        int selectimg;
                        selectimg = gameGrid->GetSelectedOption();
                        gameSelected = gameGrid->GetClickedOption();
						selectImg1=selectimg;
                }

                if (Settings.gameDisplay==carousel){
                        int selectimg;
                        selectimg = gameCarousel->GetSelectedOption();
                        gameSelected = gameCarousel->GetClickedOption();
                		selectImg1=selectimg;
				}
                if (Settings.gameDisplay==list) {
                        //Get selected game under cursor
                        int selectimg;//, promptnumber;
                        
                        selectimg = gameBrowser->GetSelectedOption();
                        gameSelected = gameBrowser->GetClickedOption();
						selectImg1=selectimg;

                        if (gameSelected > 0) //if click occured
                                selectimg = gameSelected;

                        if ((selectimg >= 0) && (selectimg < (s32) gameCnt))
                        {
                                if (selectimg != selectedold)
                                {
                                        selectedold = selectimg;//update displayed cover, game ID, and region if the selected game changes
                                        struct discHdr *header = &gameList[selectimg];
                                        snprintf (ID,sizeof(ID),"%c%c%c", header->id[0], header->id[1], header->id[2]);
                                        snprintf (IDfull,sizeof(IDfull),"%c%c%c%c%c%c", header->id[0], header->id[1], header->id[2],header->id[3], header->id[4], header->id[5]);
                                        w.Remove(&DownloadBtn);

                                        if (GameIDTxt)
                                        {
                                                w.Remove(GameIDTxt);
                                                delete GameIDTxt;
                                                GameIDTxt = NULL;
                                        }
                                        if(GameRegionTxt)
                                        {
                                                w.Remove(GameRegionTxt);
                                                delete GameRegionTxt;
                                                GameRegionTxt = NULL;
                                        }

                                        switch(header->id[3])
                                        {
                                                case 'E':
                                                sprintf(gameregion,"NTSC U");
                                                break;

                                                case 'J':
                                                sprintf(gameregion,"NTSC J");
                                                break;

                                                case 'K':
                                                sprintf(gameregion,"NTSC K");
                                                break;


                                                case 'P':
                                                case 'D':
                                                case 'F':
                                                case 'X':
                                                case 'S':
                                                case 'Y':
                                                        sprintf(gameregion,"  PAL ");
                                                break;
                                        }

                                        //load game cover
                                        if (cover)
                                        {
                                                delete cover;
                                                cover = NULL;
                                        }

                                        snprintf(imgPath, sizeof(imgPath), "%s%s.png", Settings.covers_path, IDfull);
                                        cover = new GuiImageData(imgPath,0); //load short id
                                        if (!cover->GetImage()) //if could not load the short id image
                                        {
                                                delete cover;
                                                snprintf(imgPath, sizeof(imgPath), "%s%s.png", Settings.covers_path, ID);
                                                cover = new GuiImageData(imgPath, 0); //load full id image
                                                if (!cover->GetImage())
                                                {
                                                        delete cover;
                                                        snprintf(imgPath, sizeof(imgPath), "%snoimage.png", Settings.covers_path);
                                                        cover = new GuiImageData(imgPath, nocover_png); //load no image
                                                }
                                        }

                                        if (coverImg)
                                        {
                                                delete coverImg;
                                                coverImg = NULL;
                                        }
                                        coverImg = new GuiImage(cover);
                                        coverImg->SetWidescreen(CFG.widescreen);

                                        DownloadBtn.SetImage(coverImg);// put the new image on the download button
                                        w.Append(&DownloadBtn);

                                        if ((Settings.sinfo == GameID) || (Settings.sinfo == Both)){
                                                GameIDTxt = new GuiText(IDfull, 22, (GXColor){THEME.info_r, THEME.info_g, THEME.info_b, 255});
                                                GameIDTxt->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
                                                GameIDTxt->SetPosition(THEME.id_x,THEME.id_y);
                                                GameIDTxt->SetEffect(EFFECT_FADE, 20);
                                                w.Append(GameIDTxt);
                                        }

                                        if ((Settings.sinfo == GameRegion) || (Settings.sinfo == Both)){
                                                GameRegionTxt = new GuiText(gameregion, 22, (GXColor){THEME.info_r, THEME.info_g, THEME.info_b, 255});
                                                GameRegionTxt->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
                                                GameRegionTxt->SetPosition(THEME.region_x, THEME.region_y);
                                                GameRegionTxt->SetEffect(EFFECT_FADE, 20);
                                                w.Append(GameRegionTxt);
                                        }
                                }
                        }
                }
				
				if ((gameSelected >= 0) && (gameSelected < (s32)gameCnt))
                {
                        struct discHdr *header = &gameList[gameSelected];
                        WBFS_GameSize(header->id, &size);
                        if (strlen(get_title(header)) < (MAX_CHARACTERS + 3)) {
                                sprintf(text, "%s", get_title(header));
                        }
                        else {
                                strncpy(text, get_title(header),  MAX_CHARACTERS);
                                text[MAX_CHARACTERS] = '\0';
                                strncat(text, "...", 3);
                        }

                        if (Settings.qboot == yes)//quickboot game
                        {

                                        wiilight(0);
                                        //////////save game play count////////////////
                                extern u8 favorite;
                                extern u16 count;
                                struct Game_NUM* game_num = CFG_get_game_num(header->id);

                                if (game_num)
                                        {
                                        favorite = game_num->favorite;
                                        count = game_num->count;

                                        }count+=1;

                                if(isSdInserted()) {
                                if (CFG_save_game_num(header->id))
                                {
                                        //WindowPrompt(LANGUAGE.SuccessfullySaved, 0, LANGUAGE.ok, 0,0,0);
                                }
                                else
                                {
                                        WindowPrompt(LANGUAGE.SaveFailed, 0, LANGUAGE.ok, 0,0,0);
                                }
                                }
                                ////////////end save play count//////////////

                                                struct Game_CFG* game_cfg = CFG_get_game_opt(header->id);

                        if (game_cfg)//if there are saved settings for this game use them
                        {
                            iosChoice = game_cfg->ios;
                        }
                        else// otherwise use the global settings
                        {
                            if(Settings.cios == ios222) {
                            iosChoice = i222;
                            } else {
                            iosChoice = i249;
                            }
                        }

                    int ios2;
                    switch(iosChoice)
                    {
                        case i249:
                            ios2 = 0;
                            break;

                        case i222:
                            ios2 = 1;
                            break;

                        default:
                            ios2 = 0;
                            break;
                    }

                    // if we have used the network or cios222 we need to reload the disklist
                    if(networkisinitialized == 1 || ios2 == 1 || Settings.cios == ios222)
                                        {
                                                if(ios2 == 1)
                                                {
                                                        ret = Sys_IosReload(222);

                                                        if(ret < 0)
                                                        {
                                                                WindowPrompt(LANGUAGE.YoudonthavecIOS,LANGUAGE.LoadingincIOS,LANGUAGE.ok, 0,0,0);
                                                                Sys_IosReload(249);
                                                                ios2 = 0;
                                                        }
                                                }
                                                else
                                                {
                                                        ret = Sys_IosReload(249);
                                                }
                                        }

                                        /* Set USB mode */
                                        ret = Disc_SetUSB(header->id, ios2);
                                        if (ret < 0) {
                                                sprintf(text, "%s %i", LANGUAGE.Error,ret);
                                                WindowPrompt(
                                                LANGUAGE.FailedtosetUSB,
                                                text,
                                                LANGUAGE.ok,0,0,0);
                                        }
                                        else {
                                                /* Open disc */
                                                ret = Disc_Open();
                                                if (ret < 0) {
                                                        sprintf(text, "%s %i",LANGUAGE.Error, ret);
                                                        WindowPrompt(
                                                        LANGUAGE.Failedtoboot,
                                                        text,
                                                        LANGUAGE.ok,0,0,0);
                                                }
                                                else {
                                                        menu = MENU_EXIT;
                                                }
                                        }
                                        break;
                                }
                        bool returnHere = true;// prompt to start game
                        while (returnHere)
                        {

                                returnHere = false;
                                if(Settings.wiilight != 2) wiilight(1);
                                choice = GameWindowPrompt();
                                header = &gameList[gameSelected]; //reset header

                                if(choice == 1)
                                {

                                        wiilight(0);
                                             struct Game_CFG* game_cfg = CFG_get_game_opt(header->id);
                                                if (game_cfg)//if there are saved settings for this game use them
                        {
                            iosChoice = game_cfg->ios;
                        }
                        else// otherwise use the global settings
                        {
                            if(Settings.cios == ios222) {
                            iosChoice = i222;
                            } else {
                            iosChoice = i249;
                            }
                        }


                    int ios2;
                    switch(iosChoice)
                    {
                        case i249:
                            ios2 = 0;
                            break;

                        case i222:
                            ios2 = 1;
                            break;

                        default:
                            ios2 = 0;
                            break;
                    }

                    // if we have used the network or cios222 we need to reload the disklist
                                        if(networkisinitialized == 1 || ios2 == 1 || Settings.cios == ios222)
                                        {
                                                if(ios2 == 1)
                                                {
                                                        ret = Sys_IosReload(222);
                                                        if(ret < 0)
                                                        {
                                                                WindowPrompt(LANGUAGE.YoudonthavecIOS,LANGUAGE.LoadingincIOS,LANGUAGE.ok, 0,0,0);
                                                                Sys_IosReload(249);
                                                                ios2 = 0;
                                                        }
                                                }
                                                else
                                                {
                                                        ret = Sys_IosReload(249);
                                                }
                                        }



                                        /* Set USB mode */
                                        ret = Disc_SetUSB(header->id, ios2);
                                        if (ret < 0) {
                                                sprintf(text, "%s %i", LANGUAGE.Error, ret);
                                                WindowPrompt(
                                                LANGUAGE.FailedtosetUSB,
                                                text,
                                                LANGUAGE.ok,0,0,0);
                                        }
                                        else {
                                                /* Open disc */
                                                ret = Disc_Open();
                                                if (ret < 0) {
                                                        sprintf(text, "%s %i",LANGUAGE.Error, ret);
                                                        WindowPrompt(
                                                        LANGUAGE.Failedtoboot,
                                                        text,
                                                        LANGUAGE.ok,0,0,0);

                                                }
                                                else {
                                                        menu = MENU_EXIT;
                                                }
                                        }
                                }
                                else if (choice == 2)
                                {
                                        wiilight(0);
                                        HaltGui();
                    if (Settings.gameDisplay==list) mainWindow->Remove(gameBrowser);
                    else if (Settings.gameDisplay==grid) mainWindow->Remove(gameGrid);
                    else if (Settings.gameDisplay==carousel) mainWindow->Remove(gameCarousel);
                    mainWindow->Remove(&w);
                    ResumeGui();
                                        int settret = GameSettings(header);
                                        HaltGui();
                    if (Settings.gameDisplay==list)  mainWindow->Append(gameBrowser);
                    else if (Settings.gameDisplay==grid) mainWindow->Append(gameGrid);
                    else if (Settings.gameDisplay==carousel) mainWindow->Append(gameCarousel);
                    mainWindow->Append(&w);
                    ResumeGui();
                                        if (settret == 1) //if deleted
                                        {
                                                menu = MENU_DISCLIST;
                                                break;
                                        }
                                        returnHere = true;
                                }

                                else if (choice == 3) //WBFS renaming
                                {
                                        wiilight(0);
                                                                                //enter new game title
                                        char entered[60];
                                        snprintf(entered, sizeof(entered), "%s", get_title(header));
                                        entered[59] = '\0';
                                        int result = OnScreenKeyboard(entered, 60,0);
                                        if (result == 1) {
                                        WBFS_RenameGame(header->id, entered);
                                        __Menu_GetEntries();
                                        menu = MENU_DISCLIST;
                                        }
                                }


                                else if(choice == 0)
                                        if (Settings.gameDisplay==list){gameBrowser->SetFocus(1);}
                                        else if (Settings.gameDisplay==grid){gameGrid->SetFocus(1);}
                                        else if (Settings.gameDisplay==carousel){gameCarousel->SetFocus(1);}
                        }
                }
        }

    HaltGui();
        mainWindow->RemoveAll();
        mainWindow->Append(bgImg);
        delete gameBrowser;
        gameBrowser = NULL;
        delete gameGrid;
        gameGrid = NULL;
        delete gameCarousel;
        gameCarousel = NULL;
        ResumeGui();
        return menu;
}


/****************************************************************************
 * MenuInstall
 ***************************************************************************/

static int MenuInstall()
{
	int menu = MENU_NONE;
    static struct discHdr headerdisc ATTRIBUTE_ALIGN(32);

    if(Settings.cios == ios222) {
    Disc_SetUSB(NULL, 1);
    } else {
    Disc_SetUSB(NULL, 0);
    }

    int ret, choice = 0;
	char *name;
	static char buffer[MAX_CHARACTERS + 4];

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);

    char imgPath[100];

	snprintf(imgPath, sizeof(imgPath), "%sbattery.png", CFG.theme_path);
	GuiImageData battery(imgPath, battery_png);
	snprintf(imgPath, sizeof(imgPath), "%sbattery_red.png", CFG.theme_path);
	GuiImageData batteryRed(imgPath, battery_red_png);
	snprintf(imgPath, sizeof(imgPath), "%sbattery_bar.png", CFG.theme_path);
	GuiImageData batteryBar(imgPath, battery_bar_png);

    HaltGui();
	GuiWindow w(screenwidth, screenheight);

    mainWindow->Append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
	    VIDEO_WaitVSync ();

		ret = DiscWait(LANGUAGE.InsertDisk,LANGUAGE.Waiting,LANGUAGE.Cancel,0,0);
		if (ret < 0) {
			WindowPrompt (LANGUAGE.ErrorreadingDisc,0,LANGUAGE.Back,0,0,0);
			menu = MENU_DISCLIST;
			break;
		}
		ret = Disc_Open();
		if (ret < 0) {
			WindowPrompt (LANGUAGE.CouldnotopenDisc,0,LANGUAGE.Back,0,0,0);
			menu = MENU_DISCLIST;
			break;
		}

		ret = Disc_IsWii();
		if (ret < 0) {
			choice = WindowPrompt (LANGUAGE.NotaWiiDisc,LANGUAGE.InsertaWiiDisc,LANGUAGE.ok,LANGUAGE.Back,0,0);

			if (choice == 1) {
				menu = MENU_INSTALL;
				break;
			} else
				menu = MENU_DISCLIST;
				break;
			}

		Disc_ReadHeader(&headerdisc);
		name = headerdisc.title;
		if (strlen(name) < (MAX_CHARACTERS + 3)) {
			memset(buffer, 0, sizeof(buffer));
			sprintf(name, "%s", name);
			} else {
			strncpy(buffer, name,  MAX_CHARACTERS);
			buffer[MAX_CHARACTERS] = '\0';
			strncat(buffer, "...", 3);
			sprintf(name, "%s", buffer);
		}

		ret = WBFS_CheckGame(headerdisc.id);
		if (ret) {
			WindowPrompt (LANGUAGE.Gameisalreadyinstalled,name,LANGUAGE.Back,0,0,0);
			menu = MENU_DISCLIST;
			break;
		}

		f32 freespace, used;

		WBFS_DiskSpace(&used, &freespace);
		gamesize = WBFS_EstimeGameSize()/GB_SIZE;
		char gametxt[50];

		sprintf(gametxt, "%s : %.2fGB", name, gamesize);

        wiilight(1);
		choice = WindowPrompt(LANGUAGE.Continueinstallgame,gametxt,LANGUAGE.ok,LANGUAGE.Cancel,0,0);

		if(choice == 1) {

		sprintf(gametxt, "%s", LANGUAGE.Installinggame);

		if (gamesize > freespace) {
			char errortxt[50];
			sprintf(errortxt, "%s: %.2fGB, %s: %.2fGB",LANGUAGE.GameSize, gamesize, LANGUAGE.FreeSpace, freespace);
			choice = WindowPrompt(LANGUAGE.Notenoughfreespace,errortxt,LANGUAGE.ok, LANGUAGE.Return,0,0);
			if (choice == 1) {
			    wiilight(1);
				ret = ProgressWindow(gametxt, name);
				if (ret != 0) {
					WindowPrompt (LANGUAGE.Installerror,0,LANGUAGE.Back,0,0,0);
					menu = MENU_DISCLIST;
					break;
				}
				else {
				    wiilight(1);
					__Menu_GetEntries(); //get the entries again
					WindowPrompt (LANGUAGE.Successfullyinstalled,name,LANGUAGE.ok,0,0,0);
					menu = MENU_DISCLIST;
					break;
				}
			} else {
				menu = MENU_DISCLIST;
				break;
			}

		}
		else {
			ret = ProgressWindow(gametxt, name);
			if (ret != 0) {
				WindowPrompt (LANGUAGE.Installerror,0,LANGUAGE.Back,0,0,0);
				menu = MENU_DISCLIST;
					break;
			} else {
				__Menu_GetEntries(); //get the entries again
				WindowPrompt (LANGUAGE.Successfullyinstalled,name,LANGUAGE.ok,0,0,0);
				menu = MENU_DISCLIST;
				break;
			}
		}
		} else {
		    menu = MENU_DISCLIST;
		    break;
		}

		if (shutdown == 1) {
		    wiilight(0);
			Sys_Shutdown();
		}
		if(reset == 1) {
		    wiilight(0);
			Sys_Reboot();
		}
	}

    //Turn off the WiiLight
    wiilight(0);

	HaltGui();

	mainWindow->Remove(&w);
	ResumeGui();
///	SDCard_deInit();
///	SDCard_Init();
	return menu;
}
/****************************************************************************
 * MenuFormat
 ***************************************************************************/

static int MenuFormat()
{
	USBDevice_deInit();
	int menu = MENU_NONE;
	char imgPath[100];

	OptionList options;
    partitionEntry partitions[MAX_PARTITIONS];

	u32 cnt, sector_size, selected = 2000;
	int choice, ret;
	char text[ISFS_MAXPATH];

	s32 ret2;
    ret2 = Partition_GetEntries(partitions, &sector_size);

	//create the partitionlist
    for (cnt = 0; cnt < MAX_PARTITIONS; cnt++) {
		partitionEntry *entry = &partitions[cnt];

		/* Calculate size in gigabytes */
		f32 size = entry->size * (sector_size / GB_SIZE);

        if (size) {
            sprintf(options.name[cnt], "%s %d:",LANGUAGE.Partition, cnt+1);
            sprintf (options.value[cnt],"%.2fGB", size);
        } else {
            sprintf(options.name[cnt], "%s %d:",LANGUAGE.Partition, cnt+1);
            sprintf (options.value[cnt],LANGUAGE.Cantbeformated);
        }
    }

    options.length = cnt;

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);
	GuiSound btnClick(button_click2_pcm, button_click2_pcm_size, SOUND_PCM, Settings.sfxvolume);
	snprintf(imgPath, sizeof(imgPath), "%swiimote_poweroff.png", CFG.theme_path);
	GuiImageData btnpwroff(imgPath, wiimote_poweroff_png);
	snprintf(imgPath, sizeof(imgPath), "%swiimote_poweroff_over.png", CFG.theme_path);
	GuiImageData btnpwroffOver(imgPath, wiimote_poweroff_over_png);
	snprintf(imgPath, sizeof(imgPath), "%smenu_button.png", CFG.theme_path);
	GuiImageData btnhome(imgPath, menu_button_png);
	snprintf(imgPath, sizeof(imgPath), "%smenu_button_over.png", CFG.theme_path);
	GuiImageData btnhomeOver(imgPath, menu_button_over_png);
    GuiImageData battery(battery_png);
	GuiImageData batteryRed(battery_red_png);
	GuiImageData batteryBar(battery_bar_png);

    GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

    GuiImage poweroffBtnImg(&btnpwroff);
	GuiImage poweroffBtnImgOver(&btnpwroffOver);
	poweroffBtnImg.SetWidescreen(CFG.widescreen);
	poweroffBtnImgOver.SetWidescreen(CFG.widescreen);
	GuiButton poweroffBtn(&poweroffBtnImg,&poweroffBtnImgOver, 0, 3, THEME.power_x, THEME.power_y, &trigA, &btnSoundOver, &btnClick,1);
	GuiImage exitBtnImg(&btnhome);
	GuiImage exitBtnImgOver(&btnhomeOver);
	exitBtnImg.SetWidescreen(CFG.widescreen);
	exitBtnImgOver.SetWidescreen(CFG.widescreen);
	GuiButton exitBtn(&exitBtnImg,&exitBtnImgOver, 0, 3, 0,-10, &trigA, &btnSoundOver, &btnClick,1);
	exitBtn.SetTrigger(&trigHome);

	GuiOptionBrowser optionBrowser(THEME.selection_w, THEME.selection_h, &options, CFG.theme_path, bg_options_png, 1, 0);
	optionBrowser.SetPosition(THEME.selection_x, THEME.selection_y);
	optionBrowser.SetAlignment(ALIGN_LEFT, ALIGN_CENTRE);
    optionBrowser.SetCol2Position(200);

    HaltGui();
	GuiWindow w(screenwidth, screenheight);
    w.Append(&poweroffBtn);
	w.Append(&exitBtn);

    mainWindow->Append(&w);
    mainWindow->Append(&optionBrowser);

	ResumeGui();

	while(menu == MENU_NONE)
	{
	    VIDEO_WaitVSync ();

	    selected = optionBrowser.GetClickedOption();

            for (cnt = 0; cnt < MAX_PARTITIONS; cnt++) {
                if (cnt == selected) {
                    partitionEntry *entry = &partitions[selected];
                        if (entry->size) {
                        sprintf(text, "%s %d : %.2fGB",LANGUAGE.Partition, selected+1, entry->size * (sector_size / GB_SIZE));
                        choice = WindowPrompt(
                        LANGUAGE.Doyouwanttoformat,
                        text,
                        LANGUAGE.Yes,
                        LANGUAGE.No,0,0);
                    if(choice == 1) {
                    ret = FormatingPartition(LANGUAGE.Formattingpleasewait, entry);
                        if (ret < 0) {
                            WindowPrompt(LANGUAGE.Error,LANGUAGE.Failedformating,LANGUAGE.Return,0,0,0);
                            menu = MENU_SETTINGS;

                        } else {
                            WBFS_Open();
                            sprintf(text, "%s %s", text,LANGUAGE.formated);
                            WindowPrompt(LANGUAGE.Success,text,LANGUAGE.ok,0,0,0);
                            menu = MENU_DISCLIST;
                        }
                    }
                    }
                }
            }
		if (shutdown == 1)
			Sys_Shutdown();
		if(reset == 1)
			Sys_Reboot();

	    if(poweroffBtn.GetState() == STATE_CLICKED)
		{
		    choice = WindowPrompt (LANGUAGE.ShutdownSystem,LANGUAGE.Areyousure,LANGUAGE.Yes,LANGUAGE.No,0,0);
			if(choice == 1)
			{
				Sys_Shutdown();
			}

		} else if(exitBtn.GetState() == STATE_CLICKED)
		{
		    choice = WindowPrompt (LANGUAGE.ReturntoWiiMenu,LANGUAGE.Areyousure,LANGUAGE.Yes,LANGUAGE.No,0,0);
			if(choice == 1)
			{
				Sys_LoadMenu();
			}
		}
	}


	HaltGui();

	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	ResumeGui();
	USBDevice_Init();
	return menu;
}

/****************************************************************************
 * MenuCheck
 ***************************************************************************/
static int MenuCheck()
{
	int menu = MENU_NONE;
	int i = 0;
	int choice;
	s32 ret, ret2;
	OptionList options;
	options.length = i;
	partitionEntry partitions[MAX_PARTITIONS];

		VIDEO_WaitVSync ();

        ret2 = WBFS_Init(WBFS_DEVICE_USB);
        if (ret2 < 0)
        {
            ret2 = WindowPrompt(LANGUAGE.NoUSBDevicefound,
                    LANGUAGE.Doyouwanttoretryfor30secs,
                    "cIOS249", "cIOS222",
                    LANGUAGE.BacktoWiiMenu, 0);
            if(ret2 == 1) {
				Settings.cios = ios249;
	        } else if(ret2 == 2) {
				if(Settings.cios != ios222)
				{
					//shutdown WiiMote before IOS Reload
					WPAD_Flush(0);
					WPAD_Disconnect(0);
					WPAD_Shutdown();

					//shutdown SD and USB before IOS Reload
					SDCard_deInit();
					USBDevice_deInit();
					ret = IOS_ReloadIOS(222);
					if(ret < 0)
						IOS_ReloadIOS(249);
					//reinitialize WiiMote for Prompt
					PAD_Init();
					Wpad_Init();
					WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
					WPAD_SetVRes(WPAD_CHAN_ALL, screenwidth, screenheight);
					//reinitialize SD and USB
					SDCard_Init();
					USBDevice_Init();
					if(ret < 0)
						WindowPrompt(LANGUAGE.YoudonthavecIOS,LANGUAGE.LoadingincIOS,LANGUAGE.ok, 0,0,0);
					else
						Settings.cios = ios222;
				}
            } else {
				Sys_LoadMenu();
            }

            //shutdown WiiMote before IOS Reload
            WPAD_Flush(0);
            WPAD_Disconnect(0);
            WPAD_Shutdown();

            //shutdown SD and USB before IOS Reload in DiscWait
			SDCard_deInit();
			USBDevice_deInit();

			ret2 = DiscWait(LANGUAGE.NoUSBDevice, LANGUAGE.WaitingforUSBDevice, 0, 0, 1);
			//reinitialize WiiMote for Prompt
 			PAD_Init();
            Wpad_Init();
            WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
            WPAD_SetVRes(WPAD_CHAN_ALL, screenwidth, screenheight);
			//reinitialize SD and USB
            SDCard_Init();
			USBDevice_Init();
        }
        if (ret2 < 0) {
			WindowPrompt (LANGUAGE.Error,LANGUAGE.USBDevicenotfound, LANGUAGE.ok, 0,0,0);
            Sys_LoadMenu();
		}

        ret2 = Disc_Init();
        if (ret2 < 0) {
            WindowPrompt (LANGUAGE.Error,LANGUAGE.CouldnotinitializeDIPmodule,LANGUAGE.ok, 0,0,0);
            Sys_LoadMenu();
        }

        ret2 = WBFS_Open();
        if (ret2 < 0) {
            choice = WindowPrompt(LANGUAGE.NoWBFSpartitionfound,
                                    LANGUAGE.Youneedtoformatapartition,
                                    LANGUAGE.Format,
                                    LANGUAGE.Return,0,0);
                if(choice == 0)
                {
                    Sys_LoadMenu();
                } else {
                    /* Get partition entries */
					u32 sector_size;
                    ret2 = Partition_GetEntries(partitions, &sector_size);
                    if (ret2 < 0) {
                            WindowPrompt (LANGUAGE.Nopartitionsfound,0, LANGUAGE.Restart, 0,0,0);
                            Sys_LoadMenu();

                    }
                    menu = MENU_FORMAT;
                }
        }

		if(shutdown == 1)
			Sys_Shutdown();
		if(reset == 1)
			Sys_Reboot();
		//Spieleliste laden
		__Menu_GetEntries();

        if(menu == MENU_NONE)
		menu = MENU_DISCLIST;

	return menu;
}

/****************************************************************************
 * MainMenu
 ***************************************************************************/
int MainMenu(int menu)
{

	int currentMenu = menu;
	char imgPath[100];

	#ifdef HW_RVL
	snprintf(imgPath, sizeof(imgPath), "%splayer1_point.png", CFG.theme_path);
	pointer[0] = new GuiImageData(imgPath, player1_point_png);
	snprintf(imgPath, sizeof(imgPath), "%splayer2_point.png", CFG.theme_path);
	pointer[1] = new GuiImageData(imgPath, player2_point_png);
	snprintf(imgPath, sizeof(imgPath), "%splayer3_point.png", CFG.theme_path);
	pointer[2] = new GuiImageData(imgPath, player3_point_png);
	snprintf(imgPath, sizeof(imgPath), "%splayer4_point.png", CFG.theme_path);
	pointer[3] = new GuiImageData(imgPath, player4_point_png);
	#endif

	mainWindow = new GuiWindow(screenwidth, screenheight);

	if (CFG.widescreen)
		snprintf(imgPath, sizeof(imgPath), "%swbackground.png", CFG.theme_path);
	else
		snprintf(imgPath, sizeof(imgPath), "%sbackground.png", CFG.theme_path);

	background = new GuiImageData(imgPath, CFG.widescreen? wbackground_png : background_png);

    bgImg = new GuiImage(background);
	mainWindow->Append(bgImg);

	ResumeGui();

    bgMusic = new GuiSound(bg_music_ogg, bg_music_ogg_size, SOUND_OGG, Settings.volume);
    bgMusic->SetVolume(Settings.volume);
	bgMusic->SetLoop(1); //loop music
    // startup music
    if(!strcmp("", Settings.oggload_path) || !strcmp("notset", Settings.ogg_path)) {
        bgMusic->Play();
    } else {
        bgMusic->PlayOggFile(Settings.ogg_path);
    }

	while(currentMenu != MENU_EXIT)
	{
	    SetVolumeOgg(255*(Settings.volume/100.0));

		switch (currentMenu)
		{
			case MENU_CHECK:
				currentMenu = MenuCheck();
				break;
            case MENU_FORMAT:
				currentMenu = MenuFormat();
				break;
            case MENU_INSTALL:
				currentMenu = MenuInstall();
				break;
            case MENU_SETTINGS:
				currentMenu = MenuSettings();
				break;
            case MENU_DISCLIST:
				currentMenu = MenuDiscList();
				break;
			default: // unrecognized menu
				currentMenu = MenuCheck();
				break;
		}
	}
	ExitGUIThreads();


    bgMusic->Stop();
	delete bgMusic;
	delete background;
	delete bgImg;
	delete mainWindow;
	mainWindow = NULL;
    delete pointer[0];
    delete pointer[1];
    delete pointer[2];
    delete pointer[3];
    delete GameRegionTxt;
    delete GameIDTxt;
	delete cover;
	delete coverImg;

	StopGX();
	ShutdownAudio();

    SDCard_deInit();
	USBDevice_deInit();

    struct discHdr *header = &gameList[gameSelected];
    struct Game_CFG* game_cfg = CFG_get_game_opt(header->id);

    if (game_cfg) {

        videoChoice = game_cfg->video;
        languageChoice = game_cfg->language;
        ocarinaChoice = game_cfg->ocarina;
        viChoice = game_cfg->vipatch;
        fix002 = game_cfg->errorfix002;

    } else {

        videoChoice = Settings.video;
        languageChoice = Settings.language;
        ocarinaChoice = Settings.ocarina;
        viChoice = Settings.vpatch;
        fix002 = off;
    }

    u8 errorfixer002 = 0;
    switch(fix002)
    {
        case on:
            errorfixer002 = 1;
            break;
        case off:
            errorfixer002 = 0;
            break;
    }

    switch(languageChoice)
    {
                        case ConsoleLangDefault:
                                configbytes[0] = 0xCD;
                        break;

                        case jap:
                                configbytes[0] = 0x00;
                        break;

                        case eng:
								configbytes[0] = 0x01;
                        break;

                        case ger:
                                configbytes[0] = 0x02;
                        break;

                        case fren:
                                configbytes[0] = 0x03;
                        break;

                        case esp:
                                configbytes[0] = 0x04;
                        break;

                        case it:
                                configbytes[0] = 0x05;
                        break;

                        case dut:
                                configbytes[0] = 0x06;
                        break;

                        case schin:
                                configbytes[0] = 0x07;
                        break;

                        case tchin:
                                configbytes[0] = 0x08;
                        break;

                        case kor:
                                configbytes[0] = 0x09;
                        break;
                        //wenn nicht genau klar ist welches
                        default:
                                configbytes[0] = 0xCD;
                        break;
    }

    u8 videoselected = 0;

    switch(videoChoice)
    {
                        case discdefault:
                                videoselected = 0;
                        break;

                        case pal50:
                                videoselected = 1;
                        break;

                        case pal60:
                                videoselected = 2;
                        break;

                        case ntsc:
                                videoselected = 3;
						break;

                        case systemdefault:

                                videoselected = 4;
                        break;

                        case patch:

                                videoselected = 5;
                        break;

                        default:
                                videoselected = 0;
                        break;
    }

    u32 cheat = 0;
    switch(ocarinaChoice)
    {
                        case on:
                                cheat = 1;
                        break;

                        case off:
                                cheat = 0;
                        break;

                        default:
                                cheat = 1;
                        break;
    }

    u8 vipatch = 0;
    switch(viChoice)
    {
                        case on:
                                vipatch = 1;
                        break;

                        case off:
                                vipatch = 0;
                        break;

                        default:
                                vipatch = 0;
                        break;
    }

    int ret = 0;
    ret = Disc_WiiBoot(videoselected, cheat, vipatch, Settings.patchcountrystrings, errorfixer002);
    if (ret < 0) {
        Sys_LoadMenu();
    }

    return 0;
}
