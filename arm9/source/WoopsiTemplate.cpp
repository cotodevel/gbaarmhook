// Includes
#include "WoopsiTemplate.h"
#include "woopsiheaders.h"
#include "bitmapwrapper.h"
#include "bitmap.h"
#include "graphics.h"
#include "rect.h"
#include "gadgetstyle.h"
#include "fonts/newtopaz.h"
#include "woopsistring.h"
#include "colourpicker.h"
#include "filerequester.h"
#include "soundTGDS.h"
#include "main.h"
#include "posixHandleTGDS.h"
#include "keypadTGDS.h"
#include "ipcfifoTGDSUser.h"
#include "loader.h"

//TGDS Project Specific
#include "gbaarmhookFS.h"
#include "crc32.h"

__attribute__((section(".dtcm")))
WoopsiTemplate * WoopsiTemplateProc = NULL;

void WoopsiTemplate::startup(int argc, char **argv) {
	
	Rect rect;

	/** SuperBitmap preparation **/
	// Create bitmap for superbitmap
	Bitmap* superBitmapBitmap = new Bitmap(164, 191);

	// Get a graphics object from the bitmap so that we can modify it
	Graphics* gfx = superBitmapBitmap->newGraphics();

	// Clean up
	delete gfx;

	// Create screens
	AmigaScreen* newScreen = new AmigaScreen(TGDSPROJECTNAME, Gadget::GADGET_DRAGGABLE, AmigaScreen::AMIGA_SCREEN_SHOW_DEPTH | AmigaScreen::AMIGA_SCREEN_SHOW_FLIP);
	woopsiApplication->addGadget(newScreen);
	newScreen->setPermeable(true);

	// Add child windows
	AmigaWindow* controlWindow = new AmigaWindow(0, 13, 256, 33, "Controls", Gadget::GADGET_DRAGGABLE, AmigaWindow::AMIGA_WINDOW_SHOW_DEPTH);
	newScreen->addGadget(controlWindow);

	// Controls
	controlWindow->getClientRect(rect);

	_Index = new Button(rect.x, rect.y, 41, 16, "Index");	//_Index->disable();
	_Index->setRefcon(2);
	controlWindow->addGadget(_Index);
	_Index->addGadgetEventHandler(this);
	
	_lastFile = new Button(rect.x + 41, rect.y, 17, 16, "<");
	_lastFile->setRefcon(3);
	controlWindow->addGadget(_lastFile);
	_lastFile->addGadgetEventHandler(this);
	
	_nextFile = new Button(rect.x + 41 + 17, rect.y, 17, 16, ">");
	_nextFile->setRefcon(4);
	controlWindow->addGadget(_nextFile);
	_nextFile->addGadgetEventHandler(this);
	
	_play = new Button(rect.x + 41 + 17 + 17, rect.y, 40, 16, "Play");
	_play->setRefcon(5);
	controlWindow->addGadget(_play);
	_play->addGadgetEventHandler(this);
	
	_stop = new Button(rect.x + 41 + 17 + 17 + 40, rect.y, 40, 16, "Stop");
	_stop->setRefcon(6);
	controlWindow->addGadget(_stop);
	_stop->addGadgetEventHandler(this);
	
	
	// Add File listing screen
	_fileScreen = new AmigaScreen("File List", Gadget::GADGET_DRAGGABLE, AmigaScreen::AMIGA_SCREEN_SHOW_DEPTH | AmigaScreen::AMIGA_SCREEN_SHOW_FLIP);
	woopsiApplication->addGadget(_fileScreen);
	_fileScreen->setPermeable(true);
	_fileScreen->flipToTopScreen();
	// Add screen background
	_fileScreen->insertGadget(new Gradient(0, SCREEN_TITLE_HEIGHT, 256, 192 - SCREEN_TITLE_HEIGHT, woopsiRGB(0, 31, 0), woopsiRGB(0, 0, 31)));
	
	// Create FileRequester
	_fileReq = new FileRequester(10, 10, 150, 150, "Files", "/", GADGET_DRAGGABLE | GADGET_DOUBLE_CLICKABLE);
	_fileReq->setRefcon(1);
	_fileScreen->addGadget(_fileReq);
	_fileReq->addGadgetEventHandler(this);
	currentFileRequesterIndex = 0;
	
	_MultiLineTextBoxLogger = NULL;	//destroyable TextBox
	
	enableDrawing();	// Ensure Woopsi can now draw itself
	redraw();			// Draw initial state
}

void WoopsiTemplate::shutdown() {
	Woopsi::shutdown();
}

void WoopsiTemplate::waitForAOrTouchScreenButtonMessage(MultiLineTextBox* thisLineTextBox, const WoopsiString& thisText){
	thisLineTextBox->appendText(thisText);
	scanKeys();
	while((!(keysDown() & KEY_A)) && (!(keysDown() & KEY_TOUCH))){
		scanKeys();
	}
	scanKeys();
	while((keysDown() & KEY_A) && (keysDown() & KEY_TOUCH)){
		scanKeys();
	}
}

void WoopsiTemplate::handleValueChangeEvent(const GadgetEventArgs& e) {

	// Did a gadget fire this event?
	if (e.getSource() != NULL) {
	
		// Is the gadget the file requester?
		if ((e.getSource()->getRefcon() == 1) && (((FileRequester*)e.getSource())->getSelectedOption() != NULL)) {
			
			//Handle GBAARMHook task
			WoopsiString strObj = ((FileRequester*)e.getSource())->getSelectedOption()->getText();
			memset(currentFileChosen, 0, sizeof(currentFileChosen));
			strObj.copyToCharArray(currentFileChosen);
			
			//Boot .NDS file! (homebrew only)
			char tmpName[256];
			char ext[256];
			strcpy(tmpName, currentFileChosen);
			separateExtension(tmpName, ext);
			strlwr(ext);
			if(strncmp(ext,".nds", 4) == 0){
 				char thisArgv[3][MAX_TGDSFILENAME_LENGTH];
				memset(thisArgv, 0, sizeof(thisArgv));
				strcpy(&thisArgv[0][0], TGDSPROJECTNAME);	//Arg0:	This Binary loaded
				strcpy(&thisArgv[1][0], currentFileChosen);	//Arg1:	NDS Binary reloaded
				strcpy(&thisArgv[2][0], "");					//Arg2: NDS Binary ARG0
				u32 * payload = getTGDSMBV3ARM7Bootloader();
				TGDSMultibootRunNDSPayload(currentFileChosen, (u8*)payload, 3, (char*)&thisArgv);
 			}			
			
			//Create a destroyable Textbox 
			Rect rect;
			_fileScreen->getClientRect(rect);
			_MultiLineTextBoxLogger = new MultiLineTextBox(rect.x, rect.y, 262, 170, "Loading\n...", Gadget::GADGET_DRAGGABLE, 5);
			_fileScreen->addGadget(_MultiLineTextBoxLogger);
			
			FILE * globalfileHandle = opengbarom(currentFileChosen, (char*)"r+");
			if (globalfileHandle != NULL){
				_MultiLineTextBoxLogger->removeText(0);
				_MultiLineTextBoxLogger->moveCursorToPosition(0);
				_MultiLineTextBoxLogger->appendText("File open OK: ");
				_MultiLineTextBoxLogger->appendText(strObj);
				_MultiLineTextBoxLogger->appendText("\n");
				_MultiLineTextBoxLogger->appendText("Please wait calculating CRC32... \n");
				
				//CRC32 handling
				unsigned long crc32 = -1;
				int err = Crc32_ComputeFile(globalfileHandle, &crc32);
				if((u32)crc32 != (u32)0x5F35977E){
					char arrBuild[256+1];
					sprintf(arrBuild, "%s%x\n", "Invalid file: crc32 = 0x", crc32);
					_MultiLineTextBoxLogger->appendText(WoopsiString(arrBuild));
					
					sprintf(arrBuild, "%s%x\n", "Expected: crc32 = 0x", 0x5F35977E);
					_MultiLineTextBoxLogger->appendText(WoopsiString(arrBuild));
					
					waitForAOrTouchScreenButtonMessage(_MultiLineTextBoxLogger, "Press (A) or tap touchscreen to continue. \n");
				}
				else{
				
					//int gbaofset=0;
					//_MultiLineTextBoxLogger->appendText("gbaromread @ %x:[%x]",(unsigned int)(0x08000000+gbaofset),(unsigned int)readu32gbarom(gbaofset));
					
					// u32 PATCH_BOOTCODE();
					// u32 PATCH_START();
					// u32 PATCH_HOOK_START();

					//label asm patcher	
					u32 * PATCH_BOOTCODE_PTR =((u32*)&PATCH_BOOTCODE);
					u32 * PATCH_START_PTR =((u32*)&PATCH_START);
					u32 * PATCH_HOOK_START_PTR =((u32*)&PATCH_HOOK_START);
					u32 * NDS7_RTC_PROCESS_PTR =((u32*)&NDS7_RTC_PROCESS);
					
					//_MultiLineTextBoxLogger->appendText("PATCH_BOOTCODE[0]: (%x) ",(unsigned int)(PATCH_BOOTCODE_PTR[0]));
					//_MultiLineTextBoxLogger->appendText("PATCH_START[0]: (%x) ",(unsigned int)(PATCH_START_PTR[0]));
					//_MultiLineTextBoxLogger->appendText("PATCH_HOOK_START[0]: (%x) ",(unsigned int)(PATCH_HOOK_START_PTR[0]));
					
					u8* buf_wram = (u8*)TGDSARM9Malloc(1024*192);
					
					//PATCH_BOOTCODE EXTRACT
					int PATCH_BOOTCODE_SIZE = extract_word(PATCH_BOOTCODE_PTR,(PATCH_BOOTCODE_PTR[0]),(int)(4*64),(u32*)buf_wram,0xe1a0f00d,32); //mov pc,sp end
					//_MultiLineTextBoxLogger->appendText(">PATCH_BOOTCODE EXTRACT'd (%d) opcodes",(int)PATCH_BOOTCODE_SIZE);
					
					//PATCH_START EXTRACT
					int PATCH_START_SIZE = extract_word(PATCH_START_PTR,(PATCH_START_PTR[0]),(int)(4*64),(u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)),0xe12fff1e,32); //bx lr end
					//_MultiLineTextBoxLogger->appendText(">PATCH_START EXTRACT'd (%d) ARM opcodes",(int)PATCH_START_SIZE);
					
					//PATCH_HOOK_START EXTRACT
					int PATCH_HOOK_START_SIZE = extract_word(PATCH_HOOK_START_PTR,(PATCH_HOOK_START_PTR[0]),(int)(4*64),(u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)+(PATCH_START_SIZE*4)),0xe1a0f003,32); //mov pc,r3
					//_MultiLineTextBoxLogger->appendText(">PATCH_HOOK_START EXTRACT'd (%d) ARM opcodes",(int)PATCH_HOOK_START_SIZE);
					
					//NDS7 RTC
					int NDS7_RTC_PROCESS_SIZE = extract_word(NDS7_RTC_PROCESS_PTR,(NDS7_RTC_PROCESS_PTR[0]),(int)(4*64),(u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)+(PATCH_START_SIZE*4)+(PATCH_HOOK_START_SIZE*4)),0xe12fff1e,32); //bx lr end
					//_MultiLineTextBoxLogger->appendText(">NDS7_RTC_PROCESS EXTRACT'd (%d) ARM opcodes",(int)NDS7_RTC_PROCESS_SIZE);
					
					//mostly ARM code
					//PATCH_BOOTCODE (entrypoint patch...)
					writeu32gbarom(0x00ff8000, (u32*)(buf_wram), PATCH_BOOTCODE_SIZE*4, globalfileHandle);
					
					//PATCH_START (patch on entrypoint action...)
					writeu32gbarom(0x00ff0000, (u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)), PATCH_START_SIZE*4, globalfileHandle);
					
					//PATCH_HOOK_START (IRQ handler patch)
					writeu32gbarom(0x00fe0000, (u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)+(PATCH_START_SIZE*4)), PATCH_HOOK_START_SIZE*4, globalfileHandle);
					
					//NDS7_RTC_PROCESS (IRQ handler patch)
					writeu32gbarom(0x00fe8000,(u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)+(PATCH_START_SIZE*4)+(PATCH_HOOK_START_SIZE*4)), NDS7_RTC_PROCESS_SIZE*4, globalfileHandle);
					
					//ENTRYPOINT
					//_MultiLineTextBoxLogger->appendText("\n ENTRYPOINT:(%x):[%x]",
					//(unsigned int)PATCH_ENTRYPOINT[0],(unsigned int)PATCH_ENTRYPOINT[1]);
					
					_MultiLineTextBoxLogger->appendText(">ENTRYPOINT PATCH\n");
					writeu32gbarom(0x00000204,(u32*)&PATCH_ENTRYPOINT[0], sizeof(u32), globalfileHandle); //entrypoint opcode patch
					writeu32gbarom(0x000000d0,(u32*)&PATCH_ENTRYPOINT[1], sizeof(u32), globalfileHandle); //entrypoint new address
					
					//IRQ Handler@0x0:ldr pc,=PATCH_HOOK_START
					writeu32gbarom(0x00000240,(u32*)&PATCH_ENTRYPOINT[2], sizeof(u32), globalfileHandle); //IRQ redirect opcode
					writeu32gbarom(0x00000244,(u32*)&PATCH_ENTRYPOINT[3], sizeof(u32), globalfileHandle); //IRQ redirect @0x08FE0000 new address
					
					TGDSARM9Free(buf_wram);
					closegbarom(globalfileHandle);
					waitForAOrTouchScreenButtonMessage(_MultiLineTextBoxLogger, "File patched successfully.\nPress (A) or tap touchscreen to continue.");
				}
			}
			else{ //Error
				waitForAOrTouchScreenButtonMessage(_MultiLineTextBoxLogger, "Error opening File.\nPress (A) or tap touchscreen to continue.");
			}
			
			//Destroy Textbox
			_MultiLineTextBoxLogger->invalidateVisibleRectCache();
			_fileScreen->eraseGadget(_MultiLineTextBoxLogger);
			_MultiLineTextBoxLogger->destroy();	//same as delete _MultiLineTextBoxLogger;
		}
	}
}

void WoopsiTemplate::handleLidClosed() {
	// Lid has just been closed
	_lidClosed = true;

	// Run lid closed on all gadgets
	s32 i = 0;
	while (i < _gadgets.size()) {
		_gadgets[i]->lidClose();
		i++;
	}
}

void WoopsiTemplate::handleLidOpen() {
	// Lid has just been opened
	_lidClosed = false;

	// Run lid opened on all gadgets
	s32 i = 0;
	while (i < _gadgets.size()) {
		_gadgets[i]->lidOpen();
		i++;
	}
}

void WoopsiTemplate::handleClickEvent(const GadgetEventArgs& e) {
	switch (e.getSource()->getRefcon()) {
		//_Index Event
		case 2:{
			//Get fileRequester size, if > 0, set the first element selected
			FileRequester * freqInst = _fileReq;
			FileListBox* freqListBox = freqInst->getInternalListBoxObject();
			if(freqListBox->getOptionCount() > 0){
				freqListBox->setSelectedIndex(0);
			}
			currentFileRequesterIndex = 0;
		}	
		break;
		
		//_lastFile Event
		case 3:{
			FileRequester * freqInst = _fileReq;
			FileListBox* freqListBox = freqInst->getInternalListBoxObject();
			if(currentFileRequesterIndex > 0){
				currentFileRequesterIndex--;
			}
			if(freqListBox->getOptionCount() > 0){
				freqListBox->setSelectedIndex(currentFileRequesterIndex);
			}
		}	
		break;
		
		//_nextFile Event
		case 4:{
			FileRequester * freqInst = _fileReq;
			FileListBox* freqListBox = freqInst->getInternalListBoxObject();
			if(currentFileRequesterIndex < (freqListBox->getOptionCount() - 1) ){
				currentFileRequesterIndex++;
				freqListBox->setSelectedIndex(currentFileRequesterIndex);
			}
		}	
		break;
		
		//_play Event
		case 5:{
			//Play WAV/ADPCM if selected from the FileRequester
			WoopsiString strObj = _fileReq->getSelectedOption()->getText();
			memset(currentFileChosen, 0, sizeof(currentFileChosen));
			strObj.copyToCharArray(currentFileChosen);
			pendPlay = 1;
		}	
		break;
		
		//_stop Event
		case 6:{
			pendPlay = 2;
		}	
		break;
	}
}

__attribute__((section(".dtcm")))
u32 pendPlay = 0;

char currentFileChosen[256+1];

//Called once Woopsi events are ended: TGDS Main Loop
__attribute__((section(".itcm")))
void Woopsi::ApplicationMainLoop() {
	//Earlier.. main from Woopsi SDK.
	
	//Handle TGDS stuff...
	
	
	
	switch(pendPlay){
		case(1):{
			internalCodecType = playSoundStream(currentFileChosen, _FileHandleVideo, _FileHandleAudio, TGDS_ARM7_AUDIOBUFFER_STREAM);
			if(internalCodecType == SRC_NONE){
				//stop right now
				pendPlay = 2;
			}
			else{
				pendPlay = 0;
			}
		}
		break;
		case(2):{
			stopSoundStreamUser();
			pendPlay = 0;
		}
		break;
	}
}