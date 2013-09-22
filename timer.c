#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>

#include <assert.h>
#include <ctype.h>
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
	XtIntervalId timer_id;
	struct timeval due;
	int label_width;	/*Because we don't want it to auto-resize*/

	/*One-click start buttons*/
	int num_buttons;
	Widget *button_widgets;
	int *button_times;

	/*User-set time widgets*/
	Widget user_time;
	Widget user_start;
};

static void tick(XtPointer client_data, XtIntervalId *id)
{
	struct timer_data *td=client_data;
	Arg args[2];
	struct timeval now;
	struct timeval left;

	assert(td->running);
	assert(td->timer_id == *id);

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

		/*Alert the user*/
		/*TODO: Something that's a little bit harder to
		    miss might be good.
		*/
		XBell(XtDisplay(td->clock_label), 100);

		/*Re-sensitize the start buttons*/
		for(i=0; i<td->num_buttons; i++)
			XtSetSensitive(td->button_widgets[i], True);
		XtSetSensitive(td->user_time, True);
		XtSetSensitive(td->user_start, True);

		/*Reset the display clock*/
		XtSetArg(args[0], XtNlabel, "0:00");
		XtSetArg(args[1], XtNwidth, td->label_width);
		XtSetValues(td->clock_label, args, 2);

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
		XtSetArg(args[0], XtNlabel, fmtbuf);
		XtSetArg(args[1], XtNwidth, td->label_width);
		XtSetValues(td->clock_label, args, 2);
		msec=left.tv_usec/1000;
		td->timer_id=XtAppAddTimeOut(td->app, msec, tick, td);
	}
}

static void start_proc(Widget w, XtPointer client_data, XtPointer call_data)
{
	struct timer_data *td=client_data;
	int i;
	int time=0;

	(void)call_data;

	if(w == td->user_start)
	{
		/*User-entered time*/
		Arg a;
		Widget textsrc;
		char *text;
		int min,sec;

		/*Get the text in the box*/
		XtSetArg(a, XtNtextSource, &textsrc);
		XtGetValues(td->user_time, &a, 1);
		XtSetArg(a, XtNstring, &text);
		XtGetValues(textsrc, &a, 1);

		/*The conversion is simple enough that we can convert and
		    validate in one pass.
		  We accept a decimal integer representing seconds, or a
		    colon-separated pair of decimal integers representing
		    minutes and seconds.
		  We also accidentally accept empty hours or minutes as
		    equivalent to zero (but at least one must have a nonzero
		    value), and prefixes of arbitrarily many '0's and ':'s.
		*/
		min=sec=0;
		while(*text)
		{
			if(isdigit(*text))
				sec = sec*10 + *text-'0';
			else if(*text == ':')
			{
				if(min != 0)	/*multiple ':'*/
				{
					XBell(XtDisplay(w), 100);
					return;
				}
				min=sec;
				sec=0;
			}
			else	/*invalid character*/
			{
				XBell(XtDisplay(w), 100);
				return;
			}
			text++;
		}
		time=60*min+sec;
		if(time==0)	/*well-formed but invalid time*/
		{
			XBell(XtDisplay(w), 100);
			return;
		}
	}
	else
	{
		/*One-click start*/
		for(i=0; i<td->num_buttons; i++)
		{
			if(td->button_widgets[i] == w)
			{
				time=td->button_times[i];
				break;
			}
		}
	}
	assert(time > 0);

	gettimeofday(&td->due, NULL);
	td->due.tv_sec += time;
	for(i=0; i<td->num_buttons; i++)
		XtSetSensitive(td->button_widgets[i], False);
	XtSetSensitive(td->user_time, False);
	XtSetSensitive(td->user_start, False);
	td->timer_id=XtAppAddTimeOut(td->app, 0, tick, td);
	td->running=1;
}

void destroy_timerdata(Widget w, XtPointer client_data, XtPointer call_data)
{
	struct timer_data *td=client_data;
	(void)w;
	(void)call_data;

	if(td->running)
		XtRemoveTimeOut(td->timer_id);
	XtFree((void *)td->button_widgets);
	XtFree((void *)td->button_times);
}


Widget create_timer(Widget parent, int num_buttons, char **button_labels, int *button_times)
{
	Arg args[10];
	int nargs;
	int width, maxwidth;
	int i;

	XtAppContext app=XtWidgetToApplicationContext(parent);
	Widget box;
	Widget textsrc;

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

	td->user_time=XtCreateManagedWidget("textbox", asciiTextWidgetClass, box, NULL, 0);
	/*TODO: (Xt/Xaw question) Is there a way to set the text source
	    subobject's resources on creation of the text widget?
	*/
	nargs=0;
	XtSetArg(args[nargs], XtNtextSource, &textsrc);  nargs++;
	XtGetValues(td->user_time, args, nargs);
	nargs=0;
	XtSetArg(args[nargs], XtNeditType, XawtextEdit);  nargs++;
	XtSetValues(textsrc, args, nargs);

	nargs=0;
	XtSetArg(args[nargs], XtNlabel, "Start");  nargs++;
	td->user_start=XtCreateManagedWidget("user_start", commandWidgetClass, box, args, nargs);
	XtAddCallback(td->user_start, XtNcallback, start_proc, td);

	/*Now go through and make everything the same width*/
	nargs=0;
	XtSetArg(args[nargs], "width", &width);  nargs++;
	XtGetValues(td->clock_label, args, 1);
	maxwidth=width;
	for(i=0; i<td->num_buttons; i++)
	{
		XtGetValues(td->button_widgets[i], args, 1);
		if(maxwidth < width)
			maxwidth=width;
	}
	XtGetValues(td->user_time, args, 1);
	if(maxwidth < width)
		maxwidth=width;
	XtGetValues(td->user_start, args, 1);
	if(maxwidth < width)
		maxwidth=width;

	nargs=0;
	XtSetArg(args[nargs], "width", maxwidth);  nargs++;
	XtSetValues(td->clock_label, args, 1);
	for(i=0; i<td->num_buttons; i++)
		XtSetValues(td->button_widgets[i], args, 1);
	XtSetValues(td->user_time, args, 1);
	XtSetValues(td->user_start, args, 1);
	td->label_width=maxwidth;

	XtAddCallback(box, XtNdestroyCallback, destroy_timerdata, td);

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

	int times[5]={5,30,60,90,120};
	char *labels[5]={"Five seconds", "Half minute", "Minute", "Minute and a half", "Two minutes"};
	int num_buttons=5;

	nargs=0;
	XtSetArg(args[nargs], "title", "XTimer");  nargs++;
	top=XtOpenApplication(&app, "XTimer", NULL, 0, &argc, argv, fallback, applicationShellWidgetClass, args, nargs);

	nargs=0;
	XtSetArg(args[nargs], "orientation", XtorientVertical);  nargs++;
	container=XtCreateManagedWidget("outerbox", boxWidgetClass, top, args, nargs);

	timer=create_timer(container, num_buttons, labels, times);
	quit=XtCreateManagedWidget("quit_button", commandWidgetClass, container, NULL, 0);
	XtAddCallback(quit, XtNcallback, quit_proc, &app);

	XtRealizeWidget(top);
	XtAppMainLoop(app);

	XtDestroyWidget(top);
	XtDestroyApplicationContext(app);
	return 0;
}
