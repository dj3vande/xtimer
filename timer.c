#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>

struct timer_data
{
	XtAppContext app;
	XtIntervalId id;
	struct timeval due;
	Widget out;	/*The label we're displaying the current time in*/
};

static void tick(XtPointer client_data, XtIntervalId *id)
{
	struct timer_data *td=client_data;
	Arg a;
	struct timeval now;
	struct timeval left;

	assert(td->id == *id);

	gettimeofday(&now, NULL);
	left=td->due;
	left.tv_sec -= now.tv_sec;
	left.tv_usec -= now.tv_usec;
	if(left.tv_usec < 0)
	{
		left.tv_usec += 1000000;
		left.tv_sec -= 1;
	}

	if(left.tv_sec < 0)
	{
		/*Timer is done*/
		Widget p = XtParent(td->out);
		XtSetArg(a, XtNlabel, "0:00");
		XtSetValues(td->out, &a, 1);
		XtSetSensitive(p, True);
	}

	else
	{
		int min,sec;	/*For display*/
		int msec;	/*for Xt timer*/
		char fmtbuf[16];
		/*Timer is still going*/
		if(left.tv_usec > 0)
			left.tv_sec+=1;	/*ceil*/
		min=left.tv_sec/60;
		sec=left.tv_sec%60;
		sprintf(fmtbuf, "%d:%02d", min, sec);
		XtSetArg(a, XtNlabel, fmtbuf);
		XtSetValues(td->out, &a, 1);
		msec=left.tv_usec/1000;
		td->id=XtAppAddTimeOut(td->app, msec, tick, td);
	}
}

/*TODO: Make this general enough to support "this much time" buttons, and
    a user-settable time field
*/
static void start_proc(Widget w, XtPointer client_data, XtPointer call_data)
{
	struct timer_data *td=client_data;
	Widget p=XtParent(td->out);
	int time=90;
	(void)w;
	(void)call_data;

	gettimeofday(&td->due, NULL);
	td->due.tv_sec += time;
	XtSetSensitive(p, False);
	td->id=XtAppAddTimeOut(td->app, 0, tick, td);
}


/*TODO: Make this not static*/
static struct timer_data td;

/*TODO: This probably needs to be parameterized for one-click times*/
Widget create_timer(Widget parent)
{
	Arg args[10];
	int nargs;

	XtAppContext app=XtWidgetToApplicationContext(parent);
	Widget box;
	Widget clock;
	Widget start;

	nargs=0;
	XtSetArg(args[nargs], "orientation", XtorientVertical); nargs++;
	box=XtCreateManagedWidget("timer_frame", boxWidgetClass, parent, args, nargs);

	nargs=0;
	XtSetArg(args[nargs], "label", "0:00"); nargs++;
	clock=XtCreateManagedWidget("timer_clock", labelWidgetClass, box, args, nargs);

	nargs=0;
	XtSetArg(args[nargs], "label", "Start"); nargs++;
	start=XtCreateManagedWidget("timer_start", commandWidgetClass, box, args, nargs);
	td.app=app;
	td.id=0;
	td.out=clock;
	XtAddCallback(start, XtNcallback, start_proc, &td);

	return box;
}


/*TODO: Embed the timer widget in a larger program, instead of
    making it standalone
*/
static void quit_proc(Widget w, XtPointer client_data, XtPointer call_data)
{
	XtAppContext *app=client_data;
	(void)w;
	(void)call_data;
	XtAppSetExitFlag(*app);
}

static char *fallback[]=
{
	"*Command.foreground:	white",
	"*Command.background:	blue",
	"*Label.foreground:	black",
	"*Label.background:	white",
	"*quit_button.label:	Quit",
	NULL
};

int main(int argc, char **argv)
{
	XtAppContext app;
	Widget top;
	Widget container;
	Widget timer;
	Widget quit;
	Arg args[10];
	int nargs;

	nargs=0;
	XtSetArg(args[nargs], "title", "XTimer");  nargs++;
	top=XtOpenApplication(&app, "XTimer", NULL, 0, &argc, argv, fallback, applicationShellWidgetClass, args, nargs);

	nargs=0;
	XtSetArg(args[nargs], "orientation", XtorientVertical);  nargs++;
	container=XtCreateManagedWidget("outerbox", boxWidgetClass, top, args, nargs);

	timer=create_timer(container);
	quit=XtCreateManagedWidget("quit_button", commandWidgetClass, container, NULL, 0);
	XtAddCallback(quit, XtNcallback, quit_proc, &app);

	XtRealizeWidget(top);
	XtAppMainLoop(app);

	XtDestroyWidget(top);
	XtDestroyApplicationContext(app);
	return 0;
}
