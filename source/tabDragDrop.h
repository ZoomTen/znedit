/* $Id: tabDragDrop.h,v 1.25 2004/08/20 19:33:21 n8gray Exp $ */
/*******************************************************************************
*                                                                              *
* tabDragDrop.h -- Nirvana Editor Tab Drag&Drop header file                    *
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

#ifndef NEDIT_TAB_DRAG_DROP_H_INCLUDED
#define NEDIT_TAB_DRAG_DROP_H_INCLUDED

#include <X11/Intrinsic.h>

void beginTabDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
void registerDropSite(Widget widget);
void addTabDragAction(Widget widget);

#endif /* NEDIT_TAB_DRAG_DROP_H_INCLUDED */
