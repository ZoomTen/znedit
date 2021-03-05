/* $Id: tabDragDrop.c,v 1.25 2004/08/20 19:33:21 n8gray Exp $ */
/*******************************************************************************
*                                                                              *
* tabDragDrop.c -- Nirvana Editor Tab Drag&Drop implementation file            *
*                                                                              *
* Copyright 2008 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* Feb 27, 2009                                                                 *
*                                                                              *
*******************************************************************************/

#include "tabDragDrop.h"
#include "window.h"
#include "preferences.h"

#include <Xm/DragDrop.h>
#include <Xm/PushB.h>
#include <X11/IntrinsicP.h>

#include "../Microline/XmL/Folder.h"

#include <stdlib.h>
#include <limits.h>

enum tabSide {LEFT_TAB_SIDE, RIGHT_TAB_SIDE};

static void moveTab(WindowInfo *move, WindowInfo *target, enum tabSide side);
static void dropStart(Widget w, XtPointer client_data, XtPointer call_data);
static Pixmap getMaskPixmap(Pixmap tabPixmap, int pixWidth, int pixHeigth);
static void createTabDragCursor(Widget tabWidget, Widget dragContext);
void beginTabDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void moveTabProc(XtPointer clientData, XtIntervalId *id);
static void moveTab(WindowInfo *src, WindowInfo *target, enum tabSide side );
static enum tabSide tabSideNeighborAdjustmentNum(int from, int to, enum tabSide tabSide);
static enum tabSide tabSideNeighborAdjustment(Widget widget, enum tabSide tabSide);
static enum tabSide getDropTabSide(Widget widget, int x);
static Widget getTabBarWidget(Widget widget);
static void getClosestTab(Widget w, int x, Widget *closestTab, enum tabSide *tabSide);
static void redrawTabBarProc(XtPointer clientData, XtIntervalId *id);
static void tabDragProc(Widget widget, XtPointer client_data, XtPointer call_data);
static void tabDropProc(Widget widget, XtPointer client_data, XtPointer call_data);
void registerDropSite(Widget widget);
void addTabDragAction(Widget widget);

static struct DragDropTabInfo {
    WindowInfo  *draggedWindow, *droppedWindow;
    enum tabSide sideToDropTab;
    int          redrawTabBar;
    int          lastInsertionPoint;
} ddtInfo = {NULL, NULL, RIGHT_TAB_SIDE, 0, -10000};


static struct DragIconInfo {
    Pixmap tabPixmap, tabMaskPixmap;
    Widget dragIcon;
} diInfo = {0, 0, NULL};


/* based on OM's XmRedisplayWidget */
static void redisplayWidget(Widget widget) 
{
    Region       region;
    XExposeEvent expose_event;
    XEvent      *event = (XEvent*)&expose_event;
    XtExposeProc expose_proc = widget->core.widget_class->core_class.expose;
    
    if (!expose_proc)
        return;

    region = XCreateRegion();
    
    expose_event.type       = Expose;
    expose_event.serial     = LastKnownRequestProcessed(TheDisplay);  
    expose_event.send_event = False;
    expose_event.display    = TheDisplay;
    expose_event.window     = XtWindowOfObject(widget);
    expose_event.x          = 0;
    expose_event.y          = 0;
    expose_event.width      = widget->core.width;
    expose_event.height     = widget->core.height;
    expose_event.count      = 0;

    XtAddExposureToRegion(event, region);    
    (*expose_proc)(widget, event, region);
    XDestroyRegion(region);
}

/*
** check if drop was successful (it's unsuccessful if we're dropping from another instance).
*/
static void dropStart(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmDropStartCallback start = (XmDropStartCallback)call_data;

    if (start->dropSiteStatus == XmDROP_SITE_INVALID) {
        ddtInfo.draggedWindow = NULL;
    }
}

/*
** creates a mask pixmap for the tab pixmap (tries to delete ugly corners etc)
*/
static Pixmap getMaskPixmap(Pixmap tabPixmap, int pixWidth, int pixHeigth)
{
    XImage *image = 0, *image2 = 0;
    Pixmap  tabMaskPixmap = 0;
    
    if (!tabPixmap || !pixWidth || !pixHeigth) {
        return 0;
    }
    
    tabMaskPixmap = XCreatePixmap(TheDisplay, tabPixmap, pixWidth, pixHeigth, 1);
    if (!tabMaskPixmap) {
        return 0;
    }
    
    image = XGetImage(TheDisplay, tabPixmap, 0,0, pixWidth, pixHeigth, -1, XYPixmap);
    if (!image) {
        goto FAILED;
    }
    
    image2 = XGetImage(TheDisplay, tabMaskPixmap, 0,0, pixWidth, pixHeigth, -1, XYPixmap);
    if (!image2) {
        goto FAILED;
    }

    /* now let's try to eat tab's corners */
    {
        const unsigned long topLeftPixel = XGetPixel(image, 0, 0);
        int                 x = 0, y = 0;
        
        for (y = 0; y < pixHeigth; ++y) {
            for (x = 0; x < pixWidth; ++x) {
                XPutPixel(image2, x, y, 1);
            }
        }

        /* eat top left corner */
        for (y = 0; y < pixHeigth; ++y) {
            x = 0;
            if (XGetPixel(image, x, y) != topLeftPixel) {
                break;
            }
            for ( ; x < pixWidth && XGetPixel(image, x, y) == topLeftPixel; ++x) {
                XPutPixel(image2, x, y, 0);
            }
        }

        if (y == pixHeigth || x == pixWidth) {
            goto FAILED; /* ate the whole: something is wrong - no mask */
        }

        /* eat top right corner */
        for (y = 0; y < pixHeigth; ++y) {
            x = pixWidth - 1;
            if (XGetPixel(image, x, y) != topLeftPixel) {
                break;
            }
            for ( ; x > 0 && XGetPixel(image, x, y) == topLeftPixel; --x) {
                XPutPixel(image2, x, y, 0);
            }
        }          
        if (y == 0 || x == 0) {
            goto FAILED; /* ate the whole: something is wrong - no mask */
        }

        /* ok, done with making the mask image, let's put the result to the mask pixmap */
        {
            GC gc = XCreateGC(TheDisplay, tabMaskPixmap, 0, 0);
            XPutImage(TheDisplay, tabMaskPixmap, gc, image2, 0,0, 0,0, pixWidth, pixHeigth);
            XFreeGC(TheDisplay, gc);
        }
    }
    goto DONE;
    
FAILED:
    if (tabMaskPixmap) {
        XFreePixmap(TheDisplay, tabMaskPixmap);
    }
    tabMaskPixmap = 0;

DONE:
    if (image2) {
        XDestroyImage(image2);
    }
    if (image) {
        XDestroyImage(image);
    }
    
    return tabMaskPixmap;
}

/*
** creates a drag cursor and puts it in drag context, if successful.
*/
static void createTabDragCursor(Widget tabWidget, Widget dragContext)
{
    Arg          dropIconArgs[10];
    int          n;
    const Widget tabBarWidget = XtParent(XtParent(tabWidget));
    
    /* get tab pixmap dimensions */
    int offset, pixWidth, pixHeigth ;
    XWindowAttributes attrs;
    XGetWindowAttributes(TheDisplay, XtWindow(tabWidget), &attrs);
    
    offset = attrs.y;
    pixWidth = attrs.width   + offset * 2 + attrs.border_width * 2;
    pixHeigth = attrs.height + offset     + attrs.border_width * 2;
    
    /* make a pixmap */
    if (diInfo.tabPixmap) {
        XFreePixmap(TheDisplay, diInfo.tabPixmap);
    }
    diInfo.tabPixmap = XCreatePixmap(TheDisplay, XtWindow(tabBarWidget),
                                     pixWidth, pixHeigth, attrs.depth);
    
    /* copy tab contents */
    {
        GC gc = XCreateGC(TheDisplay, diInfo.tabPixmap, 0, 0);
        XCopyArea(TheDisplay, XtWindow(tabBarWidget), diInfo.tabPixmap, gc,
                  attrs.x-offset, attrs.y-offset,
                  pixWidth, pixHeigth,
                  0, 0);
        XFreeGC(TheDisplay, gc);
    }

    /* make a mask pixmap */
    {
        Pixmap tabMaskPixmap = getMaskPixmap(diInfo.tabPixmap, pixWidth, pixHeigth);
        if (tabMaskPixmap) {
            if (diInfo.tabMaskPixmap) {
                XFreePixmap(TheDisplay, diInfo.tabMaskPixmap);
            }
            diInfo.tabMaskPixmap = tabMaskPixmap;
        }
    }
    
    /* create a drag icon */
    n = 0;
    XtSetArg(dropIconArgs[n], XmNpixmap, diInfo.tabPixmap); n++;
    XtSetArg(dropIconArgs[n], XmNdepth, attrs.depth); n++;
    XtSetArg(dropIconArgs[n], XmNwidth, pixWidth); n++;
    XtSetArg(dropIconArgs[n], XmNheight, pixHeigth); n++;
    XtSetArg(dropIconArgs[n], XmNhotX, pixWidth / 2); n++;
    XtSetArg(dropIconArgs[n], XmNhotY, pixHeigth / 2); n++;
    XtSetArg(dropIconArgs[n], XmNattachment, XmATTACH_HOT); n++;

    if (diInfo.tabMaskPixmap) {
        XtSetArg(dropIconArgs[n], XmNmask, diInfo.tabMaskPixmap); n++;
    }
    
    if (n > 10) {
        abort(); /* check if args array length is enough */
    }
    
    if (!diInfo.dragIcon)
    {
        /* create drag icon widget */
        Widget topLevelWidget = NULL, x = tabWidget;
        while ( (x = XtParent(x)) ) { /* the assigment intended here */
            topLevelWidget = x;
        }
        diInfo.dragIcon = XmCreateDragIcon(topLevelWidget, "tabDragIcon", dropIconArgs, n);
    } else {
        XtSetValues(diInfo.dragIcon, dropIconArgs, n);
    }

    XtVaSetValues(dragContext, XmNsourcePixmapIcon, diInfo.dragIcon, NULL);
    XtVaSetValues(dragContext, XmNblendModel, XmBLEND_JUST_SOURCE, NULL);
}

/*
** action procedure to support drag-n-drop of tabs for moving
** documents.
*/
void beginTabDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    Widget dragContext;
    
    if (!GetPerfDragDropTabs()) {
        return;
    }
    
    dragContext = XmGetDragContext(w, event->xbutton.time);
    XtAddCallback(dragContext, XmNdropStartCallback, dropStart, NULL);
    
    if (GetPerfDragDropTabsCursor()) {
        createTabDragCursor(w, dragContext);
    }
    
    /* set the drag window pointer to the tab that is being dragged */
    ddtInfo.draggedWindow = TabToWindow(w);
}

/* 
** timer routine that takes care of moving tabs.
*/
static void moveTabProc(XtPointer clientData, XtIntervalId *id)
{
    if (ddtInfo.droppedWindow->shell != ddtInfo.draggedWindow->shell) {
        WindowInfo *cloneWin = MoveDocument(ddtInfo.droppedWindow, ddtInfo.draggedWindow);
        if (!GetPrefSortTabs()) {
            moveTab(cloneWin, ddtInfo.droppedWindow, ddtInfo.sideToDropTab);
        }
    } else if (!GetPrefSortTabs()) {
        moveTab(ddtInfo.draggedWindow, ddtInfo.droppedWindow, ddtInfo.sideToDropTab);
    }
    /* it's important to keep this pointer NULL when there is no
       dragging, because the drop can happen from another instance
       of NEdit, that will lead to crash */
    ddtInfo.draggedWindow = NULL;
}

/*
** move a tab to one position to the right of another tab.
*/
static void moveTab(WindowInfo *src, WindowInfo *target, enum tabSide side )
{
    int         tabCount, i;
    WidgetList  tabList;
    WindowInfo *win;
    int         srcPos, targetPos;
    
    if (src == target || src->shell != target->shell) {
        return;
    }
    
    srcPos = getTabPosition(src->tab);
    targetPos = getTabPosition(target->tab);
    
    switch (side) {
        case RIGHT_TAB_SIDE:
            if (targetPos == srcPos - 1) {
                return; /* we are already on the right side */
            }
            break;
            
        case LEFT_TAB_SIDE:
            if (targetPos == srcPos + 1) {
                return; /* we are already on the left side */
            }
            break;
    }
    
    if (side == LEFT_TAB_SIDE) {
        --targetPos;
    }
    
    XtVaGetValues(target->tabBar, XmNtabWidgetList, &tabList,
            XmNtabCount, &tabCount, NULL);
    if (srcPos < targetPos) {
        /* clockwise rotate, target-tab inclusive */
        for (i = srcPos; i < targetPos; i++) {
            win = TabToWindow(tabList[i + 1]);
            win->tab = tabList[i];
            RefreshTabState(win);
        }
        src->tab = tabList[targetPos];
        RefreshTabState(src);
    } else {
        /* anti-clockwise rotate */
        for (i = srcPos; i > targetPos + 1; i--) {
            win = TabToWindow(tabList[i - 1]);
            win->tab = tabList[i];
            RefreshTabState(win);
        }
        src->tab = tabList[targetPos + 1];
        RefreshTabState(src);
    }
}

/*
** "neighbor swap"
** If we're moving a tab to its neighbor, it means we want to swap it,
** no matter which side of the neighbor the tab was dropped to.
*/
static enum tabSide tabSideNeighborAdjustmentNum(int from, int to, enum tabSide tabSide)
{
    if (from == to + 1 || from == to) {
        return LEFT_TAB_SIDE;
    } else if (from == to - 1) {
        return RIGHT_TAB_SIDE;
    } else {
        return tabSide;
    }
}

/*
** "neighbor swap"
** If we're moving a tab to its neighbor, it means we want to swap it,
** no matter which side of the neighbor the tab was dropped to.
*/
static enum tabSide tabSideNeighborAdjustment(Widget widget, enum tabSide tabSide)
{
    WindowInfo *windowUnder = 0;
    if ( ! XtIsSubclass(widget, xmPushButtonWidgetClass) ) {
        return LEFT_TAB_SIDE;
    }
    
    windowUnder = TabToWindow(widget);
    
    /* adjust only if we're moving in the same window */
    if (ddtInfo.draggedWindow->shell != windowUnder->shell) {
        return tabSide;
    }
      
    return
        tabSideNeighborAdjustmentNum(
            getTabPosition(ddtInfo.draggedWindow->tab),
            getTabPosition(windowUnder->tab),
            tabSide
        );
}

/*
** Returns side of the tab from mouse cursor (taking "neighbor swap" into account).
**
*/
static enum tabSide getDropTabSide(Widget widget, int x)
{
    Dimension tabWidth, left;
    if ( ! XtIsSubclass(widget, xmPushButtonWidgetClass) ) {
        return LEFT_TAB_SIDE;
    }
    XtVaGetValues(widget, XmNwidth, &tabWidth, NULL);
    left = x < tabWidth / 2;
    return tabSideNeighborAdjustment(widget, left ? LEFT_TAB_SIDE : RIGHT_TAB_SIDE);
}

/*
** returns tab bar widget for a widget (tab bar or tab).
*/
static Widget getTabBarWidget(Widget widget)
{
  if (XtClass(widget) == xmlFolderWidgetClass) {
      return widget;
  } else if (XtIsSubclass(widget, xmPushButtonWidgetClass)) {
      return XtParent(widget);
  } else {
      return NULL;
  }
}

/*
** returns tab and side for a widget (tab bar or tab) and mouse cursor.
*/
static void getClosestTab(Widget w, int x, Widget *closestTab, enum tabSide *tabSide)
{
    const Widget tabBar = getTabBarWidget(w);
    if (tabBar == w) {
        int     numChildren = 0;
        Widget *children = NULL;
        int     diff = INT_MAX;
        int     firstTab = 1;
        int     i = 0;
        
        XtVaGetValues(tabBar, XmNnumChildren, &numChildren, XmNchildren, &children, NULL);

        for (; i<numChildren; ++i) {
            if ( XtIsSubclass(children[i], xmPushButtonWidgetClass) ) {
                XWindowAttributes attrs;
                XGetWindowAttributes(TheDisplay, XtWindow(children[i]), &attrs);
                {
                    const int distanceToTabCenterSigned = x - (attrs.x+attrs.width/2);
                    const int distanceToTabCenter = abs(distanceToTabCenterSigned);
                    if (firstTab || distanceToTabCenter < diff) {
                        firstTab = 0;
                        diff = distanceToTabCenter;
                        *closestTab = children[i];
                        *tabSide = tabSideNeighborAdjustment(*closestTab,
                            distanceToTabCenterSigned < 0 ? LEFT_TAB_SIDE : RIGHT_TAB_SIDE);
                    }
                }
            }
        }
    } else {
        *closestTab = w;
        *tabSide = getDropTabSide(w, x);
    }
}

/*
** timer procedure to redraw the tab bar (i.e. to remove insertion site).
** When we can receive DROP_SITE_LEAVE for tab bar and then DROP_SITE_ENTER
** for tab (and vice versa) - in this case we don't want to redraw, to avoid
** flickering. But in the moment when we received DROP_SITE_LEAVE we don't
** know will there be the corresponding DROP_SITE_ENTER, so let it for timer.
*/
static void redrawTabBarProc(XtPointer clientData, XtIntervalId *id)
{
    if (ddtInfo.redrawTabBar) {
        Widget tabBar = (Widget)clientData;
        redisplayWidget(tabBar);
        ddtInfo.lastInsertionPoint = -10000;
    }
}

/*
** inverts part of window
*/
static void copySelfInverted(Window window, int x, int y, int w, int h)
{
    XGCValues gcVals;
    GC        gc = XCreateGC(TheDisplay, window, 0, &gcVals);

    gcVals.function = GXnor;
    XChangeGC(TheDisplay, gc, GCFunction, &gcVals);

    XCopyArea(TheDisplay, window, window, gc, x, y,  w, h, x, y);
    XFreeGC(TheDisplay, gc);
}

/* 
** while dragging, display where the tab will be insterted when dropped.
*/ 
static void tabDragProc(Widget widget, XtPointer client_data,
        XtPointer call_data)
{
    static Widget      lastTabBar = 0;
    Widget             tabBar = 0, tab = 0;
    enum tabSide       tabSide;
    int                spacing = 0;
    int                offset, pixWidth, pixHeigth, x, y;
    XWindowAttributes  attrs;
    XmDragProcCallback dragData = (XmDragProcCallback) call_data;
    
    if (!ddtInfo.draggedWindow) {
        dragData->dropSiteStatus = XmDROP_SITE_INVALID;
        dragData->operation = XmDROP_NOOP;
        return;
    } else {
        dragData->dropSiteStatus = XmDROP_SITE_VALID;
        dragData->operation = XmDROP_MOVE;
    }

    tabBar = getTabBarWidget(widget);
    if (!tabBar) {
        return;
    }

    if (dragData->reason == XmCR_DROP_SITE_LEAVE_MESSAGE) {
        ddtInfo.redrawTabBar = 1;
        XtAppAddTimeOut(XtWidgetToApplicationContext(widget), 0,
                        redrawTabBarProc, tabBar);
        return;
    }
    
    ddtInfo.redrawTabBar = 0; /* for redrawTabBarProc: don't redraw tab bar, we'll take care */

    /* get target tab and side */
    tab = widget;
    
    getClosestTab(widget, dragData->x, &tab, &tabSide);
    
    /* calculate position and dimensions for the insertion site */
    XtVaGetValues(tabBar, XmNspacing, &spacing, NULL);
    
    XGetWindowAttributes(TheDisplay, XtWindow(tab), &attrs);
  
    offset = attrs.y;
    pixWidth = spacing > 5 ? spacing : 5;
    pixHeigth = attrs.height + offset + attrs.border_width * 2;
    x = tabSide == LEFT_TAB_SIDE
                   ? attrs.x - offset - pixWidth / 2 - (spacing - spacing / 2)
                   : attrs.x + offset - pixWidth / 2 + spacing / 2 + attrs.border_width * 2 + attrs.width;
    y = attrs.y-offset;
    
    /* adjust position, otherwise insertion site won't be visible */
    if (spacing) {
       if (x <= 0) {
           x += pixWidth / 2;
       } else {
           XWindowAttributes tabBarAttrs;
           XGetWindowAttributes(TheDisplay, XtWindow(tabBar), &tabBarAttrs);
           if (x >= tabBarAttrs.width) {
               x -= pixWidth / 2;
           }
       }
    }
    
    /* check if the insertion site changed */
    if (ddtInfo.lastInsertionPoint == x && lastTabBar == tabBar) {
        return;
    }
  
    redisplayWidget(tabBar);
    ddtInfo.lastInsertionPoint = x;
    lastTabBar = tabBar;
  
    /* copy inverted image of the insertion site */
    copySelfInverted(XtWindow(tabBar), x, y, pixWidth, pixHeigth);
}

/* 
** initiate document movement when tabs are dragged into 
** the text area, or another tab.
*/ 
static void tabDropProc(Widget widget, XtPointer client_data,
        XtPointer call_data)
{
    XmDropProcCallback dropData;
    Widget             dc;
    Arg                args[10];
    int                n;

    dropData = (XmDropProcCallback) call_data;
    dc = dropData->dragContext;

    n = 0;
    if (!ddtInfo.draggedWindow) {
        dropData->dropSiteStatus = XmDROP_SITE_INVALID;
        dropData->operation = XmDROP_NOOP;
        XtSetArg(args[n], XmNtransferStatus, XmTRANSFER_FAILURE); n++;
        XtSetArg(args[n], XmNnumDropTransfers, 0); n++;
        XmDropTransferStart(dc, args, n);
        return;
    } else {
        dropData->dropSiteStatus = XmDROP_SITE_VALID;
        dropData->operation = XmDROP_MOVE;
        XtSetArg(args[n], XmNtransferStatus, XmTRANSFER_SUCCESS); n++;
        XtSetArg(args[n], XmNnumDropTransfers, 0); n++;
        XmDropTransferStart(dc, args, n);
    }
    
    ddtInfo.sideToDropTab = RIGHT_TAB_SIDE;
    
    /* determine the target tab based on the drop-zone:
        1. text area or another tab: it's the one,
        2. tab bar gutter: the right-most tab. */
    if (XtIsSubclass(widget, xmPushButtonWidgetClass)
          || XtClass(widget) == xmlFolderWidgetClass) {
        Widget tabWidget = 0;
        getClosestTab(widget, dropData->x, &tabWidget, &ddtInfo.sideToDropTab);
        ddtInfo.droppedWindow = TabToWindow(tabWidget);
    } else {
        ddtInfo.droppedWindow = WidgetToWindow(widget);
    }

    if (ddtInfo.droppedWindow != ddtInfo.draggedWindow) {
        /* moving document is a big operation, we delay it to avoid
           any potential conflicts with the toolkits */
        XtAppAddTimeOut(XtWidgetToApplicationContext(widget), 0,
            moveTabProc, NULL);
    } else {
        /* it's important to keep this pointer NULL when there is no
           dragging, because the drop can happen from another instance
           of NEdit, that will lead to crash */
        ddtInfo.draggedWindow = NULL;
    }
}

/*
** register a widget as drop site for tabs, for the purpose
** of moving tabs within and across windows.
*/
void registerDropSite(Widget widget)
{
#ifndef LESSTIF_VERSION
    Arg args[15];
    int n = 0;
    
    if (XtIsSubclass(widget, compositeWidgetClass)) {
        XtSetArg(args[n], XmNdropSiteType, XmDROP_SITE_COMPOSITE);
    } else {
        XtSetArg(args[n], XmNdropSiteType, XmDROP_SITE_SIMPLE); 
    }
    n++;

    XtSetArg(args[n], XmNanimationStyle, XmDRAG_UNDER_NONE); n++;
    XtSetArg(args[n], XmNnumImportTargets, 0); n++;
    XtSetArg(args[n], XmNdropSiteOperations, XmDROP_MOVE); n++;
    XtSetArg(args[n], XmNdropProc, tabDropProc); n++;
    if (GetPerfDragDropTabsAnimation()) {
        XtSetArg(args[n], XmNdragProc, tabDragProc); n++;
    }
    XmDropSiteRegister (widget, args, n);
#endif /* LESSTIF_VERSION */
}

/*
** override translation for tab widget on drag [-and-drop].
*/
void addTabDragAction(Widget widget)
{
#ifndef LESSTIF_VERSION
    static XtTranslations table = NULL;

    if (table == NULL) {
        const char *translations =
            "#override <Btn2Down>: ProcessDrag() begin_tab_drag()";
        table = XtParseTranslationTable(translations);
    }
    XtOverrideTranslations(widget, table);
#endif /* LESSTIF_VERSION */
}

