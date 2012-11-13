/*  Firmata GUI-friendly queries test
 *  Copyright 2010, Paul Stoffregen (paul@pjrc.com)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "firmata_test.h"
#endif

#include "wx/wxprec.h"
#include "firmata_test.h"
#include "serial.h"


//------------------------------------------------------------------------------
// MyFrame
//------------------------------------------------------------------------------

Serial port;
typedef struct {
	uint8_t mode;
	uint8_t analog_channel;
	uint64_t supported_modes;
	uint32_t value;
} pin_t;
pin_t pin_info[128];
wxString firmata_name;
unsigned int rx_count, tx_count;
wxMenu *port_menu;

#define MODE_INPUT    0x00
#define MODE_OUTPUT   0x01
#define MODE_ANALOG   0x02
#define MODE_PWM      0x03
#define MODE_SERVO    0x04
#define MODE_SHIFT    0x05
#define MODE_I2C      0x06

#define START_SYSEX             0xF0 // start a MIDI Sysex message
#define END_SYSEX               0xF7 // end a MIDI Sysex message
#define PIN_MODE_QUERY          0x72 // ask for current and supported pin modes
#define PIN_MODE_RESPONSE       0x73 // reply with current and supported pin modes
#define PIN_STATE_QUERY         0x6D
#define PIN_STATE_RESPONSE      0x6E
#define CAPABILITY_QUERY        0x6B
#define CAPABILITY_RESPONSE     0x6C
#define ANALOG_MAPPING_QUERY    0x69
#define ANALOG_MAPPING_RESPONSE 0x6A
#define REPORT_FIRMWARE         0x79 // report name and version of the firmware


BEGIN_EVENT_TABLE(MyFrame,wxFrame)
	EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
	EVT_MENU(wxID_EXIT, MyFrame::OnQuit)
	EVT_MENU_RANGE(9000, 9999, MyFrame::OnPort)
	EVT_CHOICE(-1, MyFrame::OnModeChange)
	EVT_IDLE(MyFrame::OnIdle)
	EVT_TOGGLEBUTTON(-1, MyFrame::OnToggleButton)
	EVT_SCROLL_THUMBTRACK(MyFrame::OnSliderDrag)
	EVT_MENU_OPEN(MyMenu::OnShowPortList)
	EVT_MENU_HIGHLIGHT(-1, MyMenu::OnHighlight)
	EVT_CLOSE(MyFrame::OnCloseWindow)
	//EVT_SIZE(MyFrame::OnSize)
END_EVENT_TABLE()

MyFrame::MyFrame( wxWindow *parent, wxWindowID id, const wxString &title,
    const wxPoint &position, const wxSize& size, long style ) :
    wxFrame( parent, id, title, position, size, style )
{
	#ifdef LOG_MSG_TO_WINDOW
	wxLog::SetActiveTarget(new wxLogWindow(this, "Debug Messages"));
	#endif
	port.Set_baud(57600);
	wxMenuBar *menubar = new wxMenuBar;
	wxMenu *menu = new wxMenu;
	menu->Append( wxID_EXIT, "Quit", "");
	menubar->Append(menu, "File");
	menu = new wxMenu;
	menubar->Append(menu, "Port");
	SetMenuBar(menubar);
	port_menu = menu;
	CreateStatusBar(1);

	scroll = new wxScrolledWindow(this);
	scroll->SetScrollRate(20, 20);
	grid = new wxFlexGridSizer(0, 3, 4, 4);
	scroll->SetSizer(grid);

	init_data();
#if 0
	// For testing only, add many controls so something
	// appears in the window without requiring any
	// actual communication...
	for (int i=0; i<80; i++) {
		pin_info[i].supported_modes = 7;
		add_pin(i);
	}
#endif
}

void MyFrame::init_data(void)
{
	grid->Clear(true);
	grid->SetRows(0);
	for (int i=0; i < 128; i++) {
		pin_info[i].mode = 255;
		pin_info[i].analog_channel = 127;
		pin_info[i].supported_modes = 0;
		pin_info[i].value = 0;
	}
	tx_count = rx_count = 0;
	firmata_name = "";
	UpdateStatus();
	new_size();
}

void MyFrame::new_size(void)
{
	grid->Layout();
	scroll->FitInside();
	Refresh();
}

void MyFrame::add_item_to_grid(int row, int col, wxWindow *item)
{
	int num_col = grid->GetCols();
	int num_row = grid->GetRows();

	if (num_row < row + 1) {
		printf("adding rows, row=%d, num_row=%d\n", row, num_row);
		grid->SetRows(row + 1);
		while (num_row < row + 1) {
			printf("  add %d static text\n", num_col);
			for (int i=0; i<num_col; i++) {
				grid->Add(new wxStaticText(scroll, -1, ""));
			}
			num_row++;
		}
	}
	int index = row * num_col + col;
	//printf("index = %d: ", index);
	wxSizerItem *existing = grid->GetItem(index);
	if (existing != NULL) {
		wxWindow *old = existing->GetWindow();
		if (old) {
			grid->Replace(old, item);
			old->Destroy();
			wxSizerItem *newitem = grid->GetItem(item);
			if (newitem) {
				newitem->SetFlag(wxALIGN_CENTER_VERTICAL);
			}
		}
	} else {
		printf("WARNING, using insert\n");
		grid->Insert(index, item);
	}
}

void MyFrame::add_pin(int pin)
{
	wxString *str = new wxString();
	str->Printf("Pin %d", pin);
	wxStaticText *pin_name = new wxStaticText(scroll, -1, *str);
	add_item_to_grid(pin, 0, pin_name);

	wxArrayString list;
	if (pin_info[pin].supported_modes & (1<<MODE_INPUT)) list.Add("Input");
	if (pin_info[pin].supported_modes & (1<<MODE_OUTPUT)) list.Add("Output");
	if (pin_info[pin].supported_modes & (1<<MODE_ANALOG)) list.Add("Analog");
	if (pin_info[pin].supported_modes & (1<<MODE_PWM)) list.Add("PWM");
	if (pin_info[pin].supported_modes & (1<<MODE_SERVO)) list.Add("Servo");
	wxPoint pos = wxPoint(0, 0);
	wxSize size = wxSize(-1, -1);
	wxChoice *modes = new wxChoice(scroll, 8000+pin, pos, size, list);
	if (pin_info[pin].mode == MODE_INPUT) modes->SetStringSelection("Input");
	if (pin_info[pin].mode == MODE_OUTPUT) modes->SetStringSelection("Output");
	if (pin_info[pin].mode == MODE_ANALOG) modes->SetStringSelection("Analog");
	if (pin_info[pin].mode == MODE_PWM) modes->SetStringSelection("PWM");
	if (pin_info[pin].mode == MODE_SERVO) modes->SetStringSelection("Servo");
	printf("create choice, mode = %d (%s)\n", pin_info[pin].mode,
		(const char *)modes->GetStringSelection());
	add_item_to_grid(pin, 1, modes);
	modes->Validate();
	wxCommandEvent cmd = wxCommandEvent(wxEVT_COMMAND_CHOICE_SELECTED, 8000+pin);
	//modes->Command(cmd);
	OnModeChange(cmd);
}

void MyFrame::UpdateStatus(void)
{
	wxString status;
	if (port.Is_open()) {
		status.Printf(port.get_name() + "    " +
			firmata_name + "    Tx:%u Rx:%u",
			tx_count, rx_count);
	} else {
		status = "Please choose serial port";
	}
	SetStatusText(status);
}	


void MyFrame::OnModeChange(wxCommandEvent &event)
{
	int id = event.GetId();
	int pin = id - 8000;
	if (pin < 0 || pin > 127) return;
	wxChoice *ch = (wxChoice *)FindWindowById(id, scroll);
	wxString sel = ch->GetStringSelection();
	printf("Mode Change, id = %d, pin=%d, ", id, pin);
	printf("Mode = %s\n", (const char *)sel);
	int mode = 255;
	if (sel.IsSameAs("Input")) mode = MODE_INPUT;
	if (sel.IsSameAs("Output")) mode = MODE_OUTPUT;
	if (sel.IsSameAs("Analog")) mode = MODE_ANALOG;
	if (sel.IsSameAs("PWM")) mode = MODE_PWM;
	if (sel.IsSameAs("Servo")) mode = MODE_SERVO;
	if (mode != pin_info[pin].mode) {
		// send the mode change message
		uint8_t buf[4];
		buf[0] = 0xF4;
		buf[1] = pin;
		buf[2] = mode;
		port.Write(buf, 3);
		tx_count += 3;
		pin_info[pin].mode = mode;
		pin_info[pin].value = 0;
	}
	// create the 3rd column control for this mode
	if (mode == MODE_OUTPUT) {
		wxToggleButton *button = new  wxToggleButton(scroll, 7000+pin, 
			pin_info[pin].value ? "High" : "Low");
		button->SetValue(pin_info[pin].value);
		add_item_to_grid(pin, 2, button);
	} else if (mode == MODE_INPUT) {
		wxStaticText *text = new wxStaticText(scroll, 5000+pin,
			pin_info[pin].value ? "High" : "Low");
		wxSize size = wxSize(128, -1);
		text->SetMinSize(size);
		text->SetWindowStyle(wxALIGN_CENTRE);
		add_item_to_grid(pin, 2, text);

	} else if (mode == MODE_ANALOG) {
		wxString val;
		val.Printf("%d", pin_info[pin].value);
		wxStaticText *text = new wxStaticText(scroll, 5000+pin, val);
		wxSize size = wxSize(128, -1);
		text->SetMinSize(size);
		text->SetWindowStyle(wxALIGN_CENTRE);
		add_item_to_grid(pin, 2, text);
	} else if (mode == MODE_PWM || mode == MODE_SERVO) {
		int maxval = (mode == MODE_PWM) ? 255 : 180;
		wxSlider *slider = new wxSlider(scroll, 6000+pin,
		  pin_info[pin].value, 0, maxval);
		wxSize size = wxSize(128, -1);
		slider->SetMinSize(size);
		add_item_to_grid(pin, 2, slider);
	}
	new_size();
}

void MyFrame::OnToggleButton(wxCommandEvent &event)
{
	int id = event.GetId();
	int pin = id - 7000;
	if (pin < 0 || pin > 127) return;
	wxToggleButton *button = (wxToggleButton *)FindWindowById(id, scroll);
	int val = button->GetValue() ? 1 : 0;
	printf("Toggle Button, id = %d, pin=%d, val=%d\n", id, pin, val);
	button->SetLabel(val ? "High" : "Low");
	pin_info[pin].value = val;
	int port_num = pin / 8;
	int port_val = 0;
	for (int i=0; i<8; i++) {
		int p = port_num * 8 + i;
		if (pin_info[p].mode == MODE_OUTPUT || pin_info[p].mode == MODE_INPUT) {
			if (pin_info[p].value) {
				port_val |= (1<<i);
			}
		}
	}
	uint8_t buf[3];
	buf[0] = 0x90 | port_num;
	buf[1] = port_val & 0x7F;
	buf[2] = (port_val >> 7) & 0x7F;
	port.Write(buf, 3);
	tx_count += 3;
	UpdateStatus();
}

void MyFrame::OnSliderDrag(wxScrollEvent &event)
{
	int id = event.GetId();
	int pin = id - 6000;
	if (pin < 0 || pin > 127) return;
	wxSlider *slider = (wxSlider *)FindWindowById(id, scroll);
	int val = slider->GetValue();
	printf("Slider Drag, id = %d, pin=%d, val=%d\n", id, pin, val);
	if (pin <= 15 && val <= 16383) {
		uint8_t buf[3];
		buf[0] = 0xE0 | pin;
		buf[1] = val & 0x7F;
		buf[2] = (val >> 7) & 0x7F;
		port.Write(buf, 3);
		tx_count += 3;
	} else {
		uint8_t buf[12];
		int len=4;
		buf[0] = 0xF0;
		buf[1] = 0x6F;
		buf[2] = pin;
		buf[3] = val & 0x7F;
		if (val > 0x00000080) buf[len++] = (val >> 7) & 0x7F;
		if (val > 0x00004000) buf[len++] = (val >> 14) & 0x7F;
		if (val > 0x00200000) buf[len++] = (val >> 21) & 0x7F;
		if (val > 0x10000000) buf[len++] = (val >> 28) & 0x7F;
		buf[len++] = 0xF7;
		port.Write(buf, len);
		tx_count += len;
	}
	UpdateStatus();
}



void MyFrame::OnPort(wxCommandEvent &event)
{
	int id = event.GetId();
	wxString name = port_menu->FindItem(id)->GetLabel();

	port.Close();
	init_data();
	printf("OnPort, id = %d, name = %s\n", id, (const char *)name);
	if (id == 9000) return;

	port.Open(name);
	port.Set_baud(57600);
	if (port.Is_open()) {
		printf("port is open\n");
		firmata_name = "";
		rx_count = tx_count = 0;
		parse_count = 0;
		parse_command_len = 0;
		UpdateStatus();
		/* 
		The startup strategy is to open the port and immediately
		send the REPORT_FIRMWARE message.  When we receive the
		firmware name reply, then we know the board is ready to
		communicate.

		For boards like Arduino which use DTR to reset, they may
		reboot the moment the port opens.  They will not hear this
		REPORT_FIRMWARE message, but when they finish booting up
		they will send the firmware message.

		For boards that do not reboot when the port opens, they
		will hear this REPORT_FIRMWARE request and send the
		response.  If this REPORT_FIRMWARE request isn't sent,
		these boards will not automatically send this info.

		Arduino boards that reboot on DTR will act like a board
		that does not reboot, if DTR is not raised when the
		port opens.  This program attempts to avoid raising
		DTR on windows.  (is this possible on Linux and Mac OS-X?)

		Either way, when we hear the REPORT_FIRMWARE reply, we
		know the board is alive and ready to communicate.
		*/
		uint8_t buf[3];
		buf[0] = START_SYSEX;
		buf[1] = REPORT_FIRMWARE; // read firmata name & version
		buf[2] = END_SYSEX;
		port.Write(buf, 3);
		tx_count += 3;
		wxWakeUpIdle();
	} else {
		printf("error opening port\n");
	}
	UpdateStatus();
}

void MyFrame::OnIdle(wxIdleEvent &event)
{
	uint8_t buf[1024];
	int r;

	//printf("Idle event\n");
	r = port.Input_wait(40);
	if (r > 0) {
		r = port.Read(buf, sizeof(buf));
		if (r < 0) {
			// error
			return;
		}
		if (r > 0) {
			// parse
			rx_count += r;
			for (int i=0; i < r; i++) {
				//printf("%02X ", buf[i]);
			}
			//printf("\n");
			Parse(buf, r);
			UpdateStatus();
		}
	} else if (r < 0) {
		return;
	}
	event.RequestMore(true);
}

void MyFrame::Parse(const uint8_t *buf, int len)
{
	const uint8_t *p, *end;

	p = buf;
	end = p + len;
	for (p = buf; p < end; p++) {
		uint8_t msn = *p & 0xF0;
		if (msn == 0xE0 || msn == 0x90 || *p == 0xF9) {
			parse_command_len = 3;
			parse_count = 0;
		} else if (msn == 0xC0 || msn == 0xD0) {
			parse_command_len = 2;
			parse_count = 0;
		} else if (*p == START_SYSEX) {
			parse_count = 0;
			parse_command_len = sizeof(parse_buf);
		} else if (*p == END_SYSEX) {
			parse_command_len = parse_count + 1;
		} else if (*p & 0x80) {
			parse_command_len = 1;
			parse_count = 0;
		}
		if (parse_count < (int)sizeof(parse_buf)) {
			parse_buf[parse_count++] = *p;
		}
		if (parse_count == parse_command_len) {
			DoMessage();
			parse_count = parse_command_len = 0;
		}
	}
}

void MyFrame::DoMessage(void)
{
	uint8_t cmd = (parse_buf[0] & 0xF0);

	//printf("message, %d bytes, %02X\n", parse_count, parse_buf[0]);

	if (cmd == 0xE0 && parse_count == 3) {
		int analog_ch = (parse_buf[0] & 0x0F);
		int analog_val = parse_buf[1] | (parse_buf[2] << 7);
		for (int pin=0; pin<128; pin++) {
			if (pin_info[pin].analog_channel == analog_ch) {
				pin_info[pin].value = analog_val;
				//printf("pin %d is A%d = %d\n", pin, analog_ch, analog_val);
				wxStaticText *text = (wxStaticText *)
				  FindWindowById(5000 + pin, scroll);
				if (text) {
					wxString val;
					val.Printf("A%d: %d", analog_ch, analog_val);
					text->SetLabel(val);
				}
				return;
			}
		}
		return;
	}
	if (cmd == 0x90 && parse_count == 3) {
		int port_num = (parse_buf[0] & 0x0F);
		int port_val = parse_buf[1] | (parse_buf[2] << 7);
		int pin = port_num * 8;
		//printf("port_num = %d, port_val = %d\n", port_num, port_val);
		for (int mask=1; mask & 0xFF; mask <<= 1, pin++) {
			if (pin_info[pin].mode == MODE_INPUT) {
				uint32_t val = (port_val & mask) ? 1 : 0;
				if (pin_info[pin].value != val) {
					printf("pin %d is %d\n", pin, val);
					wxStaticText *text = (wxStaticText *)
					  FindWindowById(5000 + pin, scroll);
					if (text) text->SetLabel(val ? "High" : "Low");
					pin_info[pin].value = val;
				}
			}
		}
		return;
	}


	if (parse_buf[0] == START_SYSEX && parse_buf[parse_count-1] == END_SYSEX) {
		// Sysex message
		if (parse_buf[1] == REPORT_FIRMWARE) {
			char name[140];
			int len=0;
			for (int i=4; i < parse_count-2; i+=2) {
				name[len++] = (parse_buf[i] & 0x7F)
				  | ((parse_buf[i+1] & 0x7F) << 7);
			}
			name[len++] = '-';
			name[len++] = parse_buf[2] + '0';
			name[len++] = '.';
			name[len++] = parse_buf[3] + '0';
			name[len++] = 0;
			firmata_name = name;
			// query the board's capabilities only after hearing the
			// REPORT_FIRMWARE message.  For boards that reset when
			// the port open (eg, Arduino with reset=DTR), they are
			// not ready to communicate for some time, so the only
			// way to reliably query their capabilities is to wait
			// until the REPORT_FIRMWARE message is heard.
			uint8_t buf[80];
			len=0;
			buf[len++] = START_SYSEX;
			buf[len++] = ANALOG_MAPPING_QUERY; // read analog to pin # info
			buf[len++] = END_SYSEX;
			buf[len++] = START_SYSEX;
			buf[len++] = CAPABILITY_QUERY; // read capabilities
			buf[len++] = END_SYSEX;
			for (int i=0; i<16; i++) {
				buf[len++] = 0xC0 | i;  // report analog
				buf[len++] = 1;
				buf[len++] = 0xD0 | i;  // report digital
				buf[len++] = 1;
			}
			port.Write(buf, len);
			tx_count += len;
		} else if (parse_buf[1] == CAPABILITY_RESPONSE) {
			int pin, i, n;
			for (pin=0; pin < 128; pin++) {
				pin_info[pin].supported_modes = 0;
			}
			for (i=2, n=0, pin=0; i<parse_count; i++) {
				if (parse_buf[i] == 127) {
					pin++;
					n = 0;
					continue;
				}
				if (n == 0) {
					// first byte is supported mode
					pin_info[pin].supported_modes |= (1<<parse_buf[i]);
				}
				n = n ^ 1;
			}
			// send a state query for for every pin with any modes
			for (pin=0; pin < 128; pin++) {
				uint8_t buf[512];
				int len=0;
				if (pin_info[pin].supported_modes) {
					buf[len++] = START_SYSEX;
					buf[len++] = PIN_STATE_QUERY;
					buf[len++] = pin;
					buf[len++] = END_SYSEX;
				}
				port.Write(buf, len);
				tx_count += len;
			}
		} else if (parse_buf[1] == ANALOG_MAPPING_RESPONSE) {
			int pin=0;
			for (int i=2; i<parse_count-1; i++) {
				pin_info[pin].analog_channel = parse_buf[i];
				pin++;
			}
			return;
		} else if (parse_buf[1] == PIN_STATE_RESPONSE && parse_count >= 6) {
			int pin = parse_buf[2];
			pin_info[pin].mode = parse_buf[3];
			pin_info[pin].value = parse_buf[4];
			if (parse_count > 6) pin_info[pin].value |= (parse_buf[5] << 7);
			if (parse_count > 7) pin_info[pin].value |= (parse_buf[6] << 14);
			add_pin(pin);
		}
		return;
	}
}




void MyFrame::OnAbout( wxCommandEvent &event )
{
    wxMessageDialog dialog( this, wxT("Firmata Test 1.0\nCopyright Paul Stoffregen"),
        wxT("About Firmata Test"), wxOK|wxICON_INFORMATION );
    dialog.ShowModal();
}

void MyFrame::OnQuit( wxCommandEvent &event )
{
     Close( true );
}

void MyFrame::OnCloseWindow( wxCloseEvent &event )
{
    // if ! saved changes -> return
    Destroy();
}

void MyFrame::OnSize( wxSizeEvent &event )
{
    event.Skip( true );
}

//------------------------------------------------------------------------------
// Port Menu
//------------------------------------------------------------------------------

MyMenu::MyMenu(const wxString& title, long style) : wxMenu(title, style)
{
}
	
void MyMenu::OnShowPortList(wxMenuEvent &event)
{
	wxMenu *menu;
	wxMenuItem *item;
	int num, any=0;

	menu = event.GetMenu();
	printf("OnShowPortList, %s\n", (const char *)menu->GetTitle());
	if (menu != port_menu) return;

	wxMenuItemList old_items = menu->GetMenuItems();
	num = old_items.GetCount();
	for (int i = old_items.GetCount() - 1; i >= 0; i--) {
		menu->Delete(old_items[i]);
	}
	menu->AppendRadioItem(9000, " (none)");
	wxArrayString list = port.port_list();
	num = list.GetCount();
	for (int i=0; i < num; i++) {
		//printf("%d: port %s\n", i, (const char *)list[i]);
		item = menu->AppendRadioItem(9001 + i, list[i]);
		if (port.Is_open() && port.get_name().IsSameAs(list[i])) {
			menu->Check(9001 + i, true);
			any = 1;
		}
	}
	if (!any) menu->Check(9000, true);
}

void MyMenu::OnHighlight(wxMenuEvent &event)
{
}


//------------------------------------------------------------------------------
// MyApp
//------------------------------------------------------------------------------

IMPLEMENT_APP(MyApp)

MyApp::MyApp()
{
}

bool MyApp::OnInit()
{
    MyFrame *frame = new MyFrame( NULL, -1, "Firmata Test", wxPoint(20,20), wxSize(400,640) );
    frame->Show( true );
    
    return true;
}

int MyApp::OnExit()
{
    return 0;
}

