//------------------------------------------------------------------------------
// emX11WindowPort.cpp
//
// Copyright (C) 2005-2009 Oliver Hamann.
//
// Homepage: http://eaglemode.sourceforge.net/
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License version 3 as published by the
// Free Software Foundation.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License version 3 for
// more details.
//
// You should have received a copy of the GNU General Public License version 3
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include <emX11/emX11WindowPort.h>
#include <X11/Xatom.h>


void emX11WindowPort::WindowFlagsChanged()
{
	int i;

	SetModalState(false);
	if (FullscreenUpdateTimer) {
		delete FullscreenUpdateTimer;
		FullscreenUpdateTimer=NULL;
	}
	if (Screen.GrabbingWinPort==this) Screen.GrabbingWinPort=NULL;
	XFreeGC(Disp,Gc);
	Gc=NULL;
	if (InputContext) {
		XDestroyIC(InputContext);
		InputContext=NULL;
	}
	XDestroyWindow(Disp,Win);
	Win=None;

	PreConstruct();

	for (i=0; i<Screen.WinPorts.GetCount(); i++) {
		if (
			Screen.WinPorts[i]->Owner==this &&
			Screen.WinPorts[i]->Win!=None
		) {
			XSetTransientForHint(Disp,Screen.WinPorts[i]->Win,Win);
		}
	}
}


void emX11WindowPort::SetPosSize(
	double x, double y, PosSizeArgSpec posSpec,
	double w, double h, PosSizeArgSpec sizeSpec
)
{
	if ((GetWindowFlags()&emWindow::WF_FULLSCREEN)!=0) {
		posSpec=PSAS_IGNORE;
		sizeSpec=PSAS_IGNORE;
	}
	if (posSpec==PSAS_IGNORE) {
		x=GetViewX();
		y=GetViewY();
	}
	else {
		if (posSpec==PSAS_WINDOW) {
			x+=BorderL;
			y+=BorderT;
		}
		x=floor(x+0.5);
		y=floor(y+0.5);
		PosForced=true;
		PosPending=true;
	}
	if (sizeSpec==PSAS_IGNORE) {
		w=GetViewWidth();
		h=GetViewHeight();
	}
	else {
		if (sizeSpec==PSAS_WINDOW) {
			w-=BorderL+BorderR;
			h-=BorderT+BorderB;
		}
		w=floor(w+0.5);
		h=floor(h+0.5);
		if (w<MinPaneW) w=MinPaneW;
		if (h<MinPaneH) h=MinPaneH;
		SizeForced=true;
		SizePending=true;
	}
	SetViewGeometry(x,y,w,h,Screen.PixelTallness);
	WakeUp();
}


void emX11WindowPort::GetBorderSizes(
	double * pL, double * pT, double * pR, double * pB
)
{
	*pL=BorderL;
	*pT=BorderT;
	*pR=BorderR;
	*pB=BorderB;
}


void emX11WindowPort::RequestFocus()
{
	if (!Focused) {
		if (PostConstructed) {
			if (!MakeViewable()) return;
			XSetInputFocus(Disp,Win,RevertToNone,CurrentTime);
		}
		Focused=true;
		SetViewFocused(true);
	}
}


void emX11WindowPort::Raise()
{
	if (PostConstructed) {
		if (!Mapped) XMapRaised(Disp,Win);
		else XRaiseWindow(Disp,Win);
	}
}


void emX11WindowPort::InvalidateTitle()
{
	TitlePending=true;
	WakeUp();
}


void emX11WindowPort::InvalidateIcon()
{
	IconPending=true;
	WakeUp();
}


void emX11WindowPort::InvalidateCursor()
{
	CursorPending=true;
	WakeUp();
}


void emX11WindowPort::InvalidatePainting(double x, double y, double w, double h)
{
	double x2,y2;

	x2=x+w;
	if (x2>ClipX2) x2=ClipX2;
	if (x<ClipX1) x=ClipX1;
	if (x>=x2) return;
	y2=y+h;
	if (y2>ClipY2) y2=ClipY2;
	if (y<ClipY1) y=ClipY1;
	if (y>=y2) return;
	MergeToInvRectList((int)x,(int)y,(int)ceil(x2),(int)ceil(y2));
	WakeUp();
}


emX11WindowPort::emX11WindowPort(emWindow & window)
	: emWindowPort(window),
	emEngine(window.GetScheduler()),
	Screen((emX11Screen&)window.GetScreen())
{
	emContext * c;
	emX11WindowPort * wp;
	emWindow * w;
	int i;

	Disp=Screen.Disp;
	Owner=NULL;
	for (c=window.GetParentContext(); c; c=c->GetParentContext()) {
		w=dynamic_cast<emWindow*>(c);
		if (!w) continue;
		if (&w->GetScreen()!=(emScreen*)&Screen) break;
		wp=dynamic_cast<emX11WindowPort*>(&(w->GetWindowPort()));
		if (!wp) continue;
		Owner=wp;
		break;
	}
	Win=None;
	InputContext=NULL;
	Gc=NULL;
	MinPaneW=1;
	MinPaneH=1;
	PaneX=0;
	PaneY=0;
	PaneW=1;
	PaneH=1;
	BorderL=0;
	BorderT=0;
	BorderR=0;
	BorderB=0;
	ClipX1=PaneX;
	ClipY1=PaneY;
	ClipX2=PaneX+PaneW;
	ClipY2=PaneY+PaneH;
	PostConstructed=false;
	Mapped=false;
	Focused=false;
	PosForced=false;
	PosPending=false;
	SizeForced=false;
	SizePending=false;
	TitlePending=false;
	IconPending=false;
	CursorPending=false;
	LaunchFeedbackSent=false;
	InvRectFreeList=NULL;
	InvRectList=NULL;
	for (i=0; i<(int)(sizeof(InvRectHeap)/sizeof(InvRect)); i++) {
		InvRectHeap[i].Next=InvRectFreeList;
		InvRectFreeList=&InvRectHeap[i];
	}
	InputStateClock=0;
	LastButtonPress=EM_KEY_NONE;
	LastButtonPressTime=0;
	LastButtonPressX=0;
	LastButtonPressY=0;
	LastButtonPressRepeat=0;
	RepeatKey=EM_KEY_NONE;
	KeyRepeat=0;
	memset(&ComposeStatus,0,sizeof(ComposeStatus));
	FullscreenUpdateTimer=NULL;
	ModalState=false;
	ModalDescendants=0;

	Screen.WinPorts.Add(this);

	SetEnginePriority(emEngine::VERY_LOW_PRIORITY);

	PreConstruct();
}


emX11WindowPort::~emX11WindowPort()
{
	int i;

	// When modifying this, the implementation of WindowFlagsChanged
	// may have to be modified too.

	SetModalState(false);
	if (FullscreenUpdateTimer) {
		delete FullscreenUpdateTimer;
		FullscreenUpdateTimer=NULL;
	}
	if (Screen.GrabbingWinPort==this) Screen.GrabbingWinPort=NULL;
	for (i=Screen.WinPorts.GetCount()-1; i>=0; i--) {
		if (Screen.WinPorts[i]==this) {
			Screen.WinPorts.Remove(i);
			break;
		}
	}
	XFreeGC(Disp,Gc);
	Gc=NULL;
	if (InputContext) {
		XDestroyIC(InputContext);
		InputContext=NULL;
	}
	XDestroyWindow(Disp,Win);
	Win=None;
}


void emX11WindowPort::PreConstruct()
{
	XSetWindowAttributes xswa;
	XWMHints xwmh;
	XClassHint xch;
	XSizeHints xsh;
	XGCValues xgcv;
	long eventMask,extraEventMask;
	double vrx,vry,vrw,vrh,d;
	int border;
	bool haveBorder;

	if ((GetWindowFlags()&emWindow::WF_FULLSCREEN)!=0) {
		Screen.LeaveFullscreenModes(&GetWindow());
		Screen.GetVisibleRect(&vrx,&vry,&vrw,&vrh);
		MinPaneW=1;
		MinPaneH=1;
		PaneX=(int)(vrx+0.5);
		PaneY=(int)(vry+0.5);
		PaneW=(int)(vrw+0.5);
		PaneH=(int)(vrh+0.5);
		BorderL=0;
		BorderT=0;
		BorderR=0;
		BorderB=0;
		haveBorder=false;
		Focused=true;
	}
	else if ((GetWindowFlags()&(emWindow::WF_POPUP|emWindow::WF_UNDECORATED))!=0) {
		Screen.GetVisibleRect(&vrx,&vry,&vrw,&vrh);
		MinPaneW=1;
		MinPaneH=1;
		PaneX=(int)(vrx+vrw*emGetDblRandom(0.22,0.28)+0.5);
		PaneY=(int)(vry+vrh*emGetDblRandom(0.22,0.28)+0.5);
		PaneW=(int)(vrw*0.5+0.5);
		PaneH=(int)(vrh*0.5+0.5);
		BorderL=0;
		BorderT=0;
		BorderR=0;
		BorderB=0;
		haveBorder=false;
		Focused=true;
	}
	else {
		Screen.LeaveFullscreenModes(&GetWindow());
		Screen.GetVisibleRect(&vrx,&vry,&vrw,&vrh);
		MinPaneW=32;
		MinPaneH=32;
		if (!Owner && (GetWindowFlags()&emWindow::WF_MODAL)==0) {
			d=emMin(vrw,vrh)*0.08;
			PaneX=(int)(vrx+d*emGetDblRandom(0.5,1.5)+0.5);
			PaneY=(int)(vry+d*emGetDblRandom(0.8,1.2)+0.5);
			PaneW=(int)(vrw-d*2.0+0.5);
			PaneH=(int)(vrh-d*2.0+0.5);
		}
		else {
			PaneX=(int)(vrx+vrw*emGetDblRandom(0.22,0.28)+0.5);
			PaneY=(int)(vry+vrh*emGetDblRandom(0.22,0.28)+0.5);
			PaneW=(int)(vrw*0.5+0.5);
			PaneH=(int)(vrh*0.5+0.5);
		}
		// Some window managers seem to expect that we would expect this:
		BorderL=3;
		BorderT=18;
		BorderR=3;
		BorderB=3;
		haveBorder=true;
		if ((GetWindowFlags()&emWindow::WF_MODAL)!=0) Focused=true;
		else Focused=false;
	}
	ClipX1=PaneX;
	ClipY1=PaneY;
	ClipX2=PaneX+PaneW;
	ClipY2=PaneY+PaneH;
	PosForced=false;
	PosPending=false;
	SizeForced=false;
	SizePending=false;
	ClearInvRectList();
	MergeToInvRectList(PaneX,PaneY,PaneX+PaneW,PaneY+PaneH);
	Title.Empty();
	TitlePending=true;
	IconPending=true;
	Cursor=-1;
	CursorPending=true;
	PostConstructed=false;
	Mapped=false;
	InputStateClock=0;
	LastButtonPress=EM_KEY_NONE;
	RepeatKey=EM_KEY_NONE;
	memset(&ComposeStatus,0,sizeof(ComposeStatus));

	memset(&xsh,0,sizeof(xsh));
	xsh.flags     =PMinSize;
	xsh.min_width =MinPaneW;
	xsh.min_height=MinPaneH;

	eventMask=
		ExposureMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|
		KeyPressMask|KeyReleaseMask|StructureNotifyMask|SubstructureNotifyMask|
		VisibilityChangeMask|FocusChangeMask
	;

	memset(&xswa,0,sizeof(xswa));
	xswa.bit_gravity=ForgetGravity;
	xswa.colormap=Screen.Colmap;
	xswa.event_mask=eventMask;

	if (haveBorder) {
		xswa.override_redirect=False;
		border=1;
	}
	else {
		xswa.override_redirect=True;
		border=0;
	}

	Win=XCreateWindow(
		Disp,
		Screen.RootWin,
		PaneX-BorderL,
		PaneY-BorderT,
		PaneW,
		PaneH,
		border,
		Screen.VisuDepth,
		InputOutput,
		Screen.Visu,
		CWBitGravity|CWColormap|CWEventMask|CWOverrideRedirect,
		&xswa
	);

	if (Owner) XSetTransientForHint(Disp,Win,Owner->Win);

	if (Screen.InputMethod) {
		InputContext=XCreateIC(
			Screen.InputMethod,
			XNInputStyle,XIMPreeditNothing|XIMStatusNothing,
			XNClientWindow,Win,
			(char*)NULL
		);
		if (InputContext==NULL) {
			emFatalError("Failed to create X input context.");
		}
	}
	else {
		InputContext=NULL;
	}

	if (InputContext) {
		XGetICValues(InputContext,XNFilterEvents,&extraEventMask,(char*)NULL);
		eventMask|=extraEventMask;
	}
	XSelectInput(Disp,Win,eventMask);

	memset(&xwmh,0,sizeof(xwmh));
	xwmh.flags=(InputHint|StateHint);
	xwmh.input=True;
	xwmh.initial_state=NormalState;

	memset(&xch,0,sizeof(xch));
	xch.res_name =(char*)GetWMResName().Get();
	xch.res_class=(char*)"EagleMode";

	XmbSetWMProperties(Disp,Win,Title.Get(),NULL,NULL,0,&xsh,&xwmh,&xch);

	XChangeProperty(
		Disp,
		Win,
		Screen.WM_PROTOCOLS,
		XA_ATOM,
		32,
		PropModeReplace,
		(const unsigned char*)&Screen.WM_DELETE_WINDOW,
		1
	);

	memset(&xgcv,0,sizeof(xgcv));
	Gc=XCreateGC(Disp,Win,0,&xgcv);

	SetViewFocused(Focused);
	SetViewGeometry(PaneX,PaneY,PaneW,PaneH,Screen.PixelTallness);

	WakeUp();
}


void emX11WindowPort::PostConstruct()
{
	int i;

	if ((GetWindowFlags()&(
		emWindow::WF_POPUP|emWindow::WF_UNDECORATED|emWindow::WF_FULLSCREEN
	))!=0) {
		XMapRaised(Disp,Win);
	}
	else {
		XMapWindow(Disp,Win);
	}

	if (Focused) {
		if (MakeViewable()) {
			if ((GetWindowFlags()&emWindow::WF_MODAL)!=0 && Owner) {
				XSetInputFocus(Disp,Win,RevertToParent,CurrentTime);
			}
			else {
				XSetInputFocus(Disp,Win,RevertToNone,CurrentTime);
			}
		}
		else {
			Focused=false;
			SetViewFocused(false);
		}
	}

	if (
		(GetWindowFlags()&emWindow::WF_FULLSCREEN)!=0 ||
		(
			(GetWindowFlags()&emWindow::WF_POPUP)!=0 &&
			(
				!Screen.GrabbingWinPort ||
				(Screen.GrabbingWinPort->GetWindowFlags()&emWindow::WF_FULLSCREEN)==0
			)
		)
	) {
		if (MakeViewable()) {
			for (i=0; ; i++) {
				if (XGrabKeyboard(
					Disp,
					Win,
					True,
					GrabModeSync,
					GrabModeAsync,
					CurrentTime
				)==GrabSuccess) break;
				if (i>10) emFatalError("XGrabKeyboard failed.");
				emWarning("XGrabKeyboard failed - trying again...");
				emSleepMS(50);
			}
			for (i=0; ; i++) {
				if (XGrabPointer(
					Disp,
					Win,
					True,
					ButtonPressMask|ButtonReleaseMask|PointerMotionMask|
					ButtonMotionMask|EnterWindowMask|LeaveWindowMask,
					GrabModeSync,
					GrabModeAsync,
					(GetWindowFlags()&emWindow::WF_FULLSCREEN)!=0 ? Win : None,
					None,
					CurrentTime
				)==GrabSuccess) break;
				if (i>10) emFatalError("XGrabPointer failed.");
				emWarning("XGrabPointer failed - trying again...");
				emSleepMS(50);
			}
			XAllowEvents(Disp,SyncPointer,CurrentTime);
			Screen.GrabbingWinPort=this;
		}
	}

	if ((GetWindowFlags()&emWindow::WF_FULLSCREEN)!=0) {
		FullscreenUpdateTimer=new emTimer(GetScheduler());
		AddWakeUpSignal(FullscreenUpdateTimer->GetSignal());
		FullscreenUpdateTimer->Start(500,true);
	}

	if ((GetWindowFlags()&emWindow::WF_MODAL)!=0) {
		SetModalState(true);
	}
}


void emX11WindowPort::HandleEvent(XEvent & event, bool forwarded)
{
	emInputEvent inputEvent;
	emX11WindowPort * wp;
	char tmp[256];
	char keymap[32];
	KeySym ks;
	emInputKey key;
	Status status;
	int i,x,y,w,h,mask,repeat,variant,len;
	double mx,my;
	bool inside;

	// Remember:
	// - Calling InputToView may delete this window port.
	// - The grab stuff is very very tricky.

	if (!forwarded) {
		if (XFilterEvent(&event,Win)) return;
	}

	switch (event.type) {
	case MotionNotify:
		mx=PaneX+event.xmotion.x+Screen.MouseWarpX;
		my=PaneY+event.xmotion.y+Screen.MouseWarpY;
		if (
			Screen.InputState.GetMouseX()!=mx ||
			Screen.InputState.GetMouseY()!=my
		) {
			Screen.InputState.SetMouse(mx,my);
			Screen.InputStateClock++;
		}
		for (i=0; i<3; i++) {
			if      (i==0) { key=EM_KEY_LEFT_BUTTON  ; mask=Button1Mask; }
			else if (i==1) { key=EM_KEY_MIDDLE_BUTTON; mask=Button2Mask; }
			else           { key=EM_KEY_RIGHT_BUTTON ; mask=Button3Mask; }
			if (Screen.InputState.Get(key) && (event.xmotion.state&mask)==0) {
				Screen.InputState.Set(key,false);
				Screen.InputStateClock++;
			}
		}
		return;
	case ButtonPress:
		mx=PaneX+event.xbutton.x+Screen.MouseWarpX;
		my=PaneY+event.xbutton.y+Screen.MouseWarpY;
		if (
			Screen.InputState.GetMouseX()!=mx ||
			Screen.InputState.GetMouseY()!=my
		) {
			Screen.InputState.SetMouse(mx,my);
			Screen.InputStateClock++;
		}
		wp=SearchOwnedPopupAt(mx,my);
		if (wp) {
			if (wp->Mapped) {
				event.xbutton.x+=PaneX-wp->PaneX;
				event.xbutton.y+=PaneY-wp->PaneY;
				wp->HandleEvent(event,true);
			}
			return;
		}
		if (ModalDescendants>0) {
			FocusModalDescendant(true);
			return;
		}
		inside=(
			mx>=PaneX && mx<PaneX+PaneW &&
			my>=PaneY && my<PaneY+PaneH
		);
		if (
			!inside &&
			Screen.GrabbingWinPort==this &&
			(GetWindowFlags()&emWindow::WF_POPUP)!=0
		) {
			XAllowEvents(Disp,ReplayPointer,CurrentTime);
			Screen.GrabbingWinPort=NULL;
			LastButtonPress=EM_KEY_NONE;
			SignalWindowClosing();
		}
		for (i=Screen.WinPorts.GetCount()-1; i>=0; i--) {
			wp=Screen.WinPorts[i];
			if (
				(wp->GetWindowFlags()&emWindow::WF_POPUP)!=0 &&
				wp!=this && !wp->IsAncestorOf(this)
			) {
				wp->SignalWindowClosing();
			}
		}
		if (!inside) return;
		if (!Focused && event.xbutton.button>=1 && event.xbutton.button<=5) {
			RequestFocus();
			Screen.UpdateKeymapAndInputState();
		}
		if (event.xbutton.button>=1 && event.xbutton.button<=3) {
			if      (event.xbutton.button==1) key=EM_KEY_LEFT_BUTTON;
			else if (event.xbutton.button==2) key=EM_KEY_MIDDLE_BUTTON;
			else                              key=EM_KEY_RIGHT_BUTTON;
			if (!Screen.InputState.Get(key)) {
				Screen.InputState.Set(key,true);
				Screen.InputStateClock++;
				if (
					key==LastButtonPress &&
					event.xbutton.time>LastButtonPressTime &&
					event.xbutton.time-LastButtonPressTime<=330 &&
					event.xbutton.x>=LastButtonPressX-10 &&
					event.xbutton.x<=LastButtonPressX+10 &&
					event.xbutton.y>=LastButtonPressY-10 &&
					event.xbutton.y<=LastButtonPressY+10
				) {
					repeat=LastButtonPressRepeat+1;
				}
				else {
					repeat=0;
				}
				LastButtonPress=key;
				LastButtonPressTime=event.xbutton.time;
				LastButtonPressX=event.xbutton.x;
				LastButtonPressY=event.xbutton.y;
				LastButtonPressRepeat=repeat;
				inputEvent.Setup(key,"",repeat,0);
				InputStateClock=Screen.InputStateClock;
				InputToView(inputEvent,Screen.InputState);
				return;
			}
		}
		else if (event.xbutton.button==4) {
			inputEvent.Setup(EM_KEY_WHEEL_UP,"",0,0);
			InputStateClock=Screen.InputStateClock;
			InputToView(inputEvent,Screen.InputState);
			return;
		}
		else if (event.xbutton.button==5) {
			inputEvent.Setup(EM_KEY_WHEEL_DOWN,"",0,0);
			InputStateClock=Screen.InputStateClock;
			InputToView(inputEvent,Screen.InputState);
			return;
		}
		return;
	case ButtonRelease:
		mx=PaneX+event.xbutton.x+Screen.MouseWarpX;
		my=PaneY+event.xbutton.y+Screen.MouseWarpY;
		if (
			Screen.InputState.GetMouseX()!=mx ||
			Screen.InputState.GetMouseY()!=my
		) {
			Screen.InputState.SetMouse(mx,my);
			Screen.InputStateClock++;
		}
		if (event.xbutton.button>=1 && event.xbutton.button<=3) {
			if      (event.xbutton.button==1) key=EM_KEY_LEFT_BUTTON;
			else if (event.xbutton.button==2) key=EM_KEY_MIDDLE_BUTTON;
			else                              key=EM_KEY_RIGHT_BUTTON;
			if (Screen.InputState.Get(key)) {
				Screen.InputState.Set(key,false);
				Screen.InputStateClock++;
				inputEvent.Eat();
				InputStateClock=Screen.InputStateClock;
				InputToView(inputEvent,Screen.InputState);
				return;
			}
		}
		return;
	case KeyPress:
		i=event.xkey.keycode/8;
		mask=1<<(event.xkey.keycode&7);
		if (i<32 && (Screen.Keymap[i]&mask)==0) {
			Screen.Keymap[i]|=mask;
			Screen.UpdateInputStateFromKeymap();
		}
		if (InputContext) {
			len=XmbLookupString(
				InputContext,
				&event.xkey,
				tmp,
				sizeof(tmp)-1,
				&ks,
				&status
			);
			if (status!=XLookupChars && status!=XLookupBoth) len=0;
			if (status!=XLookupKeySym && status!=XLookupBoth) ks=0;
		}
		else {
			len=XLookupString(
				&event.xkey,
				tmp,
				sizeof(tmp)-1,
				&ks,
				&ComposeStatus
			);
		}
		tmp[len]=0;
		key=emX11Screen::ConvertKey(ks,&variant);
		if (key==EM_KEY_NONE && !tmp[0]) return;
		repeat=0;
		if (
			key!=EM_KEY_NONE &&
			Screen.InputState.Get(key) &&
			RepeatKey==key
		) {
			repeat=KeyRepeat+1;
		}
		if (ModalDescendants>0) return;
		RepeatKey=key;
		KeyRepeat=repeat;
		inputEvent.Setup(key,tmp,repeat,variant);
		InputStateClock=Screen.InputStateClock;
		InputToView(inputEvent,Screen.InputState);
		return;
	case KeyRelease:
		memset(keymap,0,sizeof(keymap));
		XQueryKeymap(Disp,keymap);
		i=event.xkey.keycode/8;
		mask=1<<(event.xkey.keycode&7);
		if (i<32 && (keymap[i]&mask)==0) {
			RepeatKey=EM_KEY_NONE;
			if ((Screen.Keymap[i]&mask)!=0) {
				Screen.Keymap[i]&=~mask;
				Screen.UpdateInputStateFromKeymap();
			}
		}
		return;
	case Expose:
		x=event.xexpose.x;
		y=event.xexpose.y;
		w=event.xexpose.width;
		h=event.xexpose.height;
		if (x<0) { w+=x; x=0; }
		if (y<0) { h+=y; y=0; }
		if (w>PaneW-x) w=PaneW-x;
		if (h>PaneH-y) h=PaneH-y;
		if (w>0 && h>0) {
			MergeToInvRectList(PaneX+x,PaneY+y,PaneX+x+w,PaneY+y+h);
			WakeUp();
		}
		return;
	case FocusIn:
		if (
			event.xfocus.mode==NotifyNormal ||
			event.xfocus.mode==NotifyWhileGrabbed
		) {
			if (InputContext) XSetICFocus(InputContext);
			Screen.UpdateKeymapAndInputState();
			RepeatKey=EM_KEY_NONE;
			if (!Focused) {
				Focused=true;
				SetViewFocused(true);
			}
			if (ModalDescendants>0) FocusModalDescendant();
		}
		return;
	case FocusOut:
		if (
			event.xfocus.mode==NotifyNormal ||
			event.xfocus.mode==NotifyWhileGrabbed
		) {
			if (InputContext) XUnsetICFocus(InputContext);
			if (Focused) {
				Focused=false;
				SetViewFocused(false);
			}
			LastButtonPress=EM_KEY_NONE;
			RepeatKey=EM_KEY_NONE;
		}
		return;
	case ConfigureNotify:
		// The meaning of the coordinates from event.xconfigure depends on
		// the window manager. Therefore:
		GetAbsWinGeometry(Disp,Win,&x,&y,&w,&h);
		if (PaneX!=x || PaneY!=y || PaneW!=w || PaneH!=h) {
			PaneX=x;
			PaneY=y;
			PaneW=w;
			PaneH=h;
			ClipX1=PaneX;
			ClipY1=PaneY;
			ClipX2=PaneX+PaneW;
			ClipY2=PaneY+PaneH;
			ClearInvRectList();
			MergeToInvRectList(PaneX,PaneY,PaneX+PaneW,PaneY+PaneH);
			WakeUp();
			if (!PosPending && !SizePending) {
				SetViewGeometry(
					PaneX,PaneY,
					PaneW,PaneH,
					Screen.PixelTallness
				);
			}
			else if (!PosPending) {
				SetViewGeometry(
					PaneX,PaneY,
					GetViewWidth(),GetViewHeight(),
					Screen.PixelTallness
				);
			}
			else if (!SizePending) {
				SetViewGeometry(
					GetViewX(),GetViewY(),
					PaneW,PaneH,
					Screen.PixelTallness
				);
			}
			Screen.InputStateClock++;
		}
		return;
	case MapNotify:
		if (event.xmap.window==Win && !Mapped) {
			Mapped=true;
			WakeUp();
		}
		return;
	case UnmapNotify:
		if (event.xmap.window==Win && Mapped) {
			Mapped=false;
		}
		return;
	case ClientMessage:
		if (event.xclient.data.l[0]==(long)Screen.WM_DELETE_WINDOW) {
			if (ModalDescendants<=0) SignalWindowClosing();
			else FocusModalDescendant(true);
		}
		return;
	}
}


bool emX11WindowPort::FlushInputState()
{
	if (Focused && InputStateClock!=Screen.InputStateClock) {
		InputStateClock=Screen.InputStateClock;
		emInputEvent inputEvent;
		InputToView(inputEvent,Screen.InputState);
		return true;
	}
	else {
		return false;
	}
}


bool emX11WindowPort::Cycle()
{
	XWindowAttributes attr;
	XSizeHints xsh;
	emString str;
	emCursor cur;
	::Window win;
	emX11WindowPort * wp;
	double vrx,vry,vrw,vrh,fx,fy,fw,fh;
	int i,x,y,w,h;

	if (
		FullscreenUpdateTimer &&
		IsSignaled(FullscreenUpdateTimer->GetSignal())
	) {
		Screen.GetVisibleRect(&vrx,&vry,&vrw,&vrh);
		if (
			fabs(PaneX-vrx)>0.51 || fabs(PaneY-vry)>0.51 ||
			fabs(PaneW-vrw)>0.51 || fabs(PaneH-vrh)>0.51
		) {
			PosForced=true;
			PosPending=true;
			SizeForced=true;
			SizePending=true;
			SetViewGeometry(vrx,vry,vrw,vrh,Screen.PixelTallness);
		}

		// Workaround for lots of focus problems with several window managers:
		if (Screen.GrabbingWinPort==this) {
			XGetInputFocus(Disp,&win,&i);
			wp=NULL;
			for (i=Screen.WinPorts.GetCount()-1; i>=0; i--) {
				if (Screen.WinPorts[i]->Win==win) {
					wp=Screen.WinPorts[i];
					break;
				}
			}
			if (wp==this) {
				if (!Focused) {
					Focused=true;
					SetViewFocused(true);
					emWarning("emX11WindowPort: Focus workaround 1 applied.");
				}
			}
			else {
				while (wp) {
					if (wp==this) break;
					wp=wp->Owner;
				}
				if (
					!wp &&
					XGetWindowAttributes(Disp,Win,&attr) &&
					attr.map_state==IsViewable
				) {
					XSetInputFocus(Disp,Win,RevertToNone,CurrentTime);
					emWarning("emX11WindowPort: Focus workaround 2 applied.");
				}
			}
		}
	}

	if (
		!PostConstructed && !PosForced && Owner &&
		(GetWindowFlags()&emWindow::WF_FULLSCREEN)==0
	) {
		Screen.GetVisibleRect(&vrx,&vry,&vrw,&vrh);
		fx=Owner->GetViewX()-Owner->BorderL;
		fy=Owner->GetViewY()-Owner->BorderT;
		fw=Owner->GetViewWidth()+Owner->BorderL+Owner->BorderR;
		fh=Owner->GetViewHeight()+Owner->BorderT+Owner->BorderB;
		fx+=fw*0.5;
		fy+=fh*0.5;
		fw=GetViewWidth()+BorderL+BorderR;
		fh=GetViewHeight()+BorderT+BorderB;
		fx-=fw*0.5+emGetDblRandom(-0.03,0.03)*vrw;
		fy-=fh*0.5+emGetDblRandom(-0.03,0.03)*vrh;
		if (fx>vrx+vrw-fw) fx=vrx+vrw-fw;
		if (fy>vry+vrh-fh) fy=vry+vrh-fh;
		if (fx<vrx) fx=vrx;
		if (fy<vry) fy=vry;
		SetViewGeometry(
			fx+BorderL,fy+BorderT,
			GetViewWidth(),GetViewHeight(),
			Screen.PixelTallness
		);
		PosPending=true;
		PosForced=true;
	}

	if (PosPending || SizePending) {
		x=((int)GetViewX())-BorderL;
		y=((int)GetViewY())-BorderT;
		w=(int)GetViewWidth();
		h=(int)GetViewHeight();
		memset(&xsh,0,sizeof(xsh));
		xsh.flags     =PMinSize;
		xsh.min_width =MinPaneW;
		xsh.min_height=MinPaneH;
		if (PosForced) {
			xsh.flags|=PPosition|USPosition;
			xsh.x=x;
			xsh.y=y;
		}
		if (SizeForced) {
			xsh.flags|=PSize|USSize;
			xsh.width=w;
			xsh.height=h;
		}
		XSetWMNormalHints(Disp,Win,&xsh);
		if (PosPending && SizePending) {
			XMoveResizeWindow(Disp,Win,x,y,w,h);
		}
		else if (PosPending) {
			XMoveWindow(Disp,Win,x,y);
		}
		else {
			XResizeWindow(Disp,Win,w,h);
		}
		PosPending=false;
		SizePending=false;
	}

	if (TitlePending) {
		str=GetWindowTitle();
		if (Title!=str) {
			Title=str;
			XmbSetWMProperties(Disp,Win,Title.Get(),NULL,NULL,0,NULL,NULL,NULL);
		}
		TitlePending=false;
	}

	if (IconPending) {
		SetIconProperty(GetWindowIcon());
		IconPending=false;
	}

	if (CursorPending) {
		cur=GetViewCursor();
		if (Cursor!=cur) {
			Cursor=cur;
			XDefineCursor(Disp,Win,Screen.GetXCursor(cur));
		}
		CursorPending=false;
	}

	if (!PostConstructed) {
		PostConstruct();
		PostConstructed=true;
	}

	if (InvRectList && Mapped) {
		UpdatePainting();
		if (!LaunchFeedbackSent) {
			LaunchFeedbackSent=true;
			SendLaunchFeedback();
		}
	}

	return false;
}


void emX11WindowPort::UpdatePainting()
{
	InvRect * r;
	int i,rx1,ry1,rx2,ry2,x,y,w,h;

	if (!Screen.UsingXShm) {
		while ((r=InvRectList)!=NULL) {
			rx1=r->x1;
			ry1=r->y1;
			rx2=r->x2;
			ry2=r->y2;
			InvRectList=r->Next;
			r->Next=InvRectFreeList;
			InvRectFreeList=r;
			y=ry1;
			do {
				h=ry2-y;
				if (h>Screen.BufHeight) h=Screen.BufHeight;
				x=rx1;
				do {
					w=rx2-x;
					if (w>Screen.BufWidth) w=Screen.BufWidth;
					PaintView(emPainter(Screen.BufPainter[0],0,0,w,h,-x,-y,1,1),0);
					XPutImage(
						Disp,
						Win,
						Gc,
						Screen.BufImg[0],
						0,0,
						x-PaneX,y-PaneY,
						w,h
					);
					x+=w;
				} while (x<rx2);
				y+=h;
			} while (y<ry2);
		}
		return;
	}

	while ((r=InvRectList)!=NULL) {
		rx1=r->x1;
		ry1=r->y1;
		rx2=r->x2;
		ry2=r->y2;
		InvRectList=r->Next;
		r->Next=InvRectFreeList;
		InvRectFreeList=r;
		y=ry1;
		do {
			h=ry2-y;
			if (h>Screen.BufHeight) h=Screen.BufHeight;
			x=rx1;
			do {
				w=rx2-x;
				if (w>Screen.BufWidth) w=Screen.BufWidth;
				for (;;) {
					if (!Screen.BufActive[0]) { i=0; break; }
					if (Screen.BufImg[1] && !Screen.BufActive[1]) { i=1; break; }
					Screen.WaitBufs();
				}
				PaintView(emPainter(Screen.BufPainter[i],0,0,w,h,-x,-y,1,1),0);
				XShmPutImage(
					Disp,
					Win,
					Gc,
					Screen.BufImg[i],
					0,0,
					x-PaneX,y-PaneY,
					w,h,
					True
				);
				XFlush(Disp);
				Screen.BufActive[i]=true;
				x+=w;
			} while (x<rx2);
			y+=h;
		} while (y<ry2);
	}
	while (Screen.BufActive[0] || Screen.BufActive[1]) {
		Screen.WaitBufs();
	}
}


void emX11WindowPort::ClearInvRectList()
{
	InvRect * r;

	while ((r=InvRectList)!=NULL) {
		InvRectList=r->Next;
		r->Next=InvRectFreeList;
		InvRectFreeList=r;
	}
}


void emX11WindowPort::MergeToInvRectList(int x1, int y1, int x2, int y2)
{
	InvRect * * pr;
	InvRect * r;
	int rx1,ry1,rx2,ry2;

	pr=&InvRectList;
	for (;;) {
		r=*pr;
		if (!r || (ry1=r->y1)>y2) break;
		if ((ry2=r->y2)<y1 || (rx1=r->x1)>x2 || (rx2=r->x2)<x1) {
			pr=&r->Next;
		}
		else if (rx1>=x1 && rx2<=x2 && ry1>=y1 && ry2<=y2) {
			*pr=r->Next;
			r->Next=InvRectFreeList;
			InvRectFreeList=r;
		}
		else if (rx1<=x1 && rx2>=x2 && ry1<=y1 && ry2>=y2) {
			return;
		}
		else if (rx1==x1 && rx2==x2) {
			if (y1>ry1) y1=ry1;
			if (y2<ry2) y2=ry2;
			*pr=r->Next;
			r->Next=InvRectFreeList;
			InvRectFreeList=r;
			pr=&InvRectList;
		}
		else if (ry1<y2 && ry2>y1) {
			*pr=r->Next;
			r->Next=InvRectFreeList;
			InvRectFreeList=r;
			if (ry1<y1) {
				MergeToInvRectList(rx1,ry1,rx2,y1);
			}
			else if (y1<ry1) {
				MergeToInvRectList(x1,y1,x2,ry1);
				y1=ry1;
			}
			if (ry2>y2) {
				MergeToInvRectList(rx1,y2,rx2,ry2);
			}
			else if (ry2<y2) {
				MergeToInvRectList(x1,ry2,x2,y2);
				y2=ry2;
			}
			if (x1>rx1) x1=rx1;
			if (x2<rx2) x2=rx2;
			pr=&InvRectList;
		}
		else {
			pr=&r->Next;
		}
	}

	pr=&InvRectList;
	for (;;) {
		r=*pr;
		if (!r || r->y1>y1 || (r->y1==y1 && r->x1>x1)) break;
		pr=&r->Next;
	}
	r=InvRectFreeList;
	if (!r) {
		while ((r=InvRectList)!=NULL) {
			if (x1>r->x1) x1=r->x1;
			if (x2<r->x2) x2=r->x2;
			if (y1>r->y1) y1=r->y1;
			if (y2<r->y2) y2=r->y2;
			InvRectList=r->Next;
			r->Next=InvRectFreeList;
			InvRectFreeList=r;
		}
		pr=&InvRectList;
		r=InvRectFreeList;
	}
	InvRectFreeList=r->Next;
	r->x1=x1;
	r->y1=y1;
	r->x2=x2;
	r->y2=y2;
	r->Next=*pr;
	*pr=r;
}


bool emX11WindowPort::MakeViewable()
{
	XWindowAttributes attr;
	int i;

	for (i=0; i<100; i++) {
		XSync(Disp,False);
		if (!XGetWindowAttributes(Disp,Win,&attr)) break;
		if (attr.map_state==IsViewable) return true;
		if (i==0) XMapWindow(Disp,Win);
		else emSleepMS(10);
	}
	emWarning("emX11WindowPort::MakeViewable failed.");
	return false;
}


emX11WindowPort * emX11WindowPort::SearchOwnedPopupAt(double x, double y)
{
	emX11WindowPort * wp;
	int i;

	for (i=Screen.WinPorts.GetCount()-1; i>=0; i--) {
		wp=Screen.WinPorts[i];
		if (
			wp->Owner==this &&
			(wp->GetWindowFlags()&emWindow::WF_POPUP)!=0 &&
			x>=wp->GetViewX() && x<wp->GetViewX()+wp->GetViewWidth() &&
			y>=wp->GetViewY() && y<wp->GetViewY()+wp->GetViewHeight()
		) return wp;
	}
	return NULL;
}


bool emX11WindowPort::IsAncestorOf(emX11WindowPort * wp)
{
	while (wp) {
		wp=wp->Owner;
		if (wp==this) return true;
	}
	return false;
}


void emX11WindowPort::SetModalState(bool modalState)
{
	emX11WindowPort * wp;

	if (ModalState!=modalState) {
		for (wp=Owner; wp; wp=wp->Owner) {
			if (modalState) wp->ModalDescendants++;
			else wp->ModalDescendants--;
		}
		ModalState=modalState;
	}
}


void emX11WindowPort::FocusModalDescendant(bool flash)
{
	emX11WindowPort * wp;
	int i;

	for (i=Screen.WinPorts.GetCount()-1; i>=0; i--) {
		wp=Screen.WinPorts[i];
		if (wp->ModalState && wp->ModalDescendants<=0) {
			for (; wp && wp!=this; wp=wp->Owner);
			if (wp==this) break;
		}
	}
	if (i<0) return;
	wp=Screen.WinPorts[i];

	wp->RequestFocus();

	if (flash) wp->Flash();
}


void emX11WindowPort::Flash()
{
	XGCValues gcv;
	GC gc;
	unsigned long pix;
	int i,d;

	Screen.Beep();

	gc=XCreateGC(Disp,Win,0,&gcv);
	d=emMin(2,emMin(PaneW,PaneH));
	for (i=0; i<2; i++) {
		if ((i&1)==0) pix=BlackPixel(Disp,Screen.Scrn);
		else pix=WhitePixel(Disp,Screen.Scrn);
		XSetForeground(Disp,gc,pix);
		XFillRectangle(Disp,Win,gc,0,0,PaneW,d);
		XFillRectangle(Disp,Win,gc,0,0,d,PaneH);
		XFillRectangle(Disp,Win,gc,PaneW-d,0,d,PaneH);
		XFillRectangle(Disp,Win,gc,0,PaneH-d,PaneW,d);
		XFlush(Disp);
		emSleepMS(20);
	}
	XFreeGC(Disp,gc);
	InvalidatePainting(PaneX,PaneY,PaneW,PaneH);
}


void emX11WindowPort::SetIconProperty(const emImage & icon)
{
	emImage image;
	unsigned long * data, * t, * e;
	const emByte * s;
	int w,h,n;

	if (icon.IsEmpty()) return;
	image=icon.GetConverted(4);
	w=image.GetWidth();
	h=image.GetHeight();
	n=w*h+2;
	data = new unsigned long[n];
	data[0]=w;
	data[1]=h;
	s=image.GetMap();
	t=data+2;
	e=data+n;
	while (t<e) {
		*t=
			(((emUInt32)s[0])<<16) |
			(((emUInt32)s[1])<<8)  |
			s[2]                   |
			(((emUInt32)s[3])<<24)
		;
		t++;
		s+=4;
	}

	XChangeProperty(
		Disp,
		Win,
		Screen._NET_WM_ICON,
		XA_CARDINAL,
		32,
		PropModeReplace,
		(const unsigned char*)data,
		n
	);

	delete [] data;
}


void emX11WindowPort::SendLaunchFeedback()
{
	const char * id;
	Atom _NET_STARTUP_INFO_BEGIN, _NET_STARTUP_INFO;
	XEvent xevent;
	emString msg;
	int i,sz;

	id=getenv("DESKTOP_STARTUP_ID");
	if (!id || !*id) return;
	msg=emString::Format("remove: ID=%s",id);
	unsetenv("DESKTOP_STARTUP_ID");
	sz=msg.GetLen()+1;
	_NET_STARTUP_INFO_BEGIN=XInternAtom(Disp,"_NET_STARTUP_INFO_BEGIN",False);
	_NET_STARTUP_INFO=XInternAtom(Disp,"_NET_STARTUP_INFO",False);
	for (i=0; i<sz; i+=20) {
		memset(&xevent,0,sizeof(xevent));
		xevent.xclient.type=ClientMessage;
		xevent.xclient.display=Disp;
		xevent.xclient.window=Win;
		if (i==0) xevent.xclient.message_type=_NET_STARTUP_INFO_BEGIN;
		else xevent.xclient.message_type=_NET_STARTUP_INFO;
		xevent.xclient.format=8;
		memcpy(xevent.xclient.data.b,msg.Get()+i,emMin(sz-i,20));
		XSendEvent(Disp,Screen.RootWin,False,PropertyChangeMask,&xevent);
	}
}


void emX11WindowPort::GetAbsWinGeometry(
	Display * disp, ::Window win, int * pX, int *pY, int * pW, int *pH
)
{
	::Window current,parent,root;
	::Window * children;
	XWindowAttributes attr;
	unsigned int nchildren;

	*pX=0;
	*pY=0;
	*pW=100;
	*pH=100;
	for (current=win;;) {
		if (!XGetWindowAttributes(disp,current,&attr)) break;
		*pX+=attr.x;
		*pY+=attr.y;
		if (current==win) {
			*pW=attr.width;
			*pH=attr.height;
		}
		if (!XQueryTree(
			disp,
			current,
			&root,
			&parent,
			&children,
			&nchildren
		)) break;
		if (children) XFree(children);
		if (root==parent) break;
		current=parent;
	}
}
