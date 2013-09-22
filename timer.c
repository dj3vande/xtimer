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
	/*General data*/
	XtAppContext app;

	/*Clock data*/
	int running;
	Widget clock_label;
	XtIntervalId id;
	struct timeval due;

	/*One-click start buttons*/
	int num_buttons;
	Widget *button_widgets;
	int *button_times;

	/*TODO: User-input start button*/
};

static void tick(XtPointer client_data, XtIntervalId *id)
{
	struct timer_data *td=client_data;
	Arg a;
	struct timeval now;
	struct timeval left;

	assert(td->running);
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
		int i;

		/*TODO: Audible alert*/

		/*Re-sensitize the start buttons*/
		for(i=0; i<td->num_buttons; i++)
			XtSetSensitive(td->button_widgets[i], True);

		/*Reset the display clock*/
		XtSetArg(a, XtNlabel, "0:00");
		XtSetValues(td->clock_label, &a, 1);

		td->running=0;
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
		XtSetValues(td->clock_label, &a, 1);
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
	int i;
	int time=0;

	(void)w;
	(void)call_data;

	for(i=0; i<td->num_buttons; i++)
	{
		if(td->button_widgets[i] == w)
		{
			time=td->button_times[i];
			break;
		}
	}
	assert(time > 0);

	gettimeofday(&td->due, NULL);
	td->due.tv_sec += time;
	for(i=0; i<td->num_buttons; i++)
		XtSetSensitive(td->button_widgets[i], False);
	td->id=XtAppAddTimeOut(td->app, 0, tick, td);
	td->running=1;
}


/*TODO: This probably needs to be parameterized for one-click times*/
Widget create_timer(Widget parent, int num_buttons, char **button_labels, int *button_times)
{
	Arg args[10];
	int nargs;

	XtAppContext app=XtWidgetToApplicationContext(parent);
	Widget box;

	struct timer_data *td;

	td=(void *)XtMalloc(sizeof *td);
	td->button_widgets=(void *)XtMalloc(num_buttons * sizeof *td->button_widgets);
	td->button_times=(void *)XtMalloc(num_buttons * sizeof *td->button_times);
	td->app=app;
	td->running=0;

	nargs=0;
	XtSetArg(args[nargs], "orientation", XtorientVertical); nargs++;
	box=XtCreateManagedWidget("timer_frame", boxWidgetClass, parent, args, nargs);

	nargs=0;
	XtSetArg(args[nargs], "label", "0:00"); nargs++;
	td->clock_label=XtCreateManagedWidget("timer_clock", labelWidgetClass, box, args, nargs);

	for(td->num_buttons=0; td->num_buttons<num_buttons; td->num_buttons++)
	{
		nargs=0;
		XtSetArg(args[nargs], "label", button_labels[td->num_buttons]); nargs++;
		td->button_widgets[td->num_buttons]=XtCreateManagedWidget("timer_start", commandWidgetClass, box, args, nargs);
		td->button_times[td->num_buttons]=button_times[td->num_buttons];
		XtAddCallback(td->button_widgets[td->num_buttons], XtNcallback, start_proc, td);
	}

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

	int timer_time=60;
	char *timer_label="Minute";

	nargs=0;
	XtSetArg(args[nargs], "title", "XTimer");  nargs++;
	top=XtOpenApplication(&app, "XTimer", NULL, 0, &argc, argv, fallback, applicationShellWidgetClass, args, nargs);

	nargs=0;
	XtSetArg(args[nargs], "orientation", XtorientVertical);  nargs++;
	container=XtCreateManagedWidget("outerbox", boxWidgetClass, top, args, nargs);

	timer=create_timer(container, 1, &timer_label, &timer_time);
	quit=XtCreateManagedWidget("quit_button", commandWidgetClass, container, NULL, 0);
	XtAddCallback(quit, XtNcallback, quit_proc, &app);

	XtRealizeWidget(top);
	XtAppMainLoop(app);

	XtDestroyWidget(top);
	XtDestroyApplicationContext(app);
	return 0;
}
