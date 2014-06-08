/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "url.h"
#include "radiant_i18n.h"
#include "iradiant.h"
#include "gtkutil/dialog.h"

#if defined(WIN32) || defined(_WIN32)
#include <gdk/gdkwin32.h>
#include <shellapi.h>
static inline bool open_url(const char* url) {
	return ShellExecute((HWND)GDK_WINDOW_HWND (GTK_WIDGET(GlobalRadiant().getMainWindow())->window), "open", url, 0, 0, SW_SHOW) > (HINSTANCE)32;
}
#elif defined(__linux__) || defined(__FreeBSD__)
#include <stdlib.h>
static inline bool open_url (const char* url)
{
	char command[2 * PATH_MAX];
	snprintf(command, sizeof(command), "x-www-browser \"%s\" &", url);
	return (system(command) == 0);
}
#elif defined(__APPLE__)
#include <stdlib.h>
static inline bool open_url(const char* url) {
	char command[2 * PATH_MAX];
	snprintf (command, sizeof(command), "open \"%s\" &", url);
	return (system(command) == 0);
}
#else
#error "unknown platform"
#endif

void OpenURL (const char* url)
{
	// let's put a little comment
	globalOutputStream() << "OpenURL: " << url << "\n";
	if (!open_url(url)) {
		gtkutil::errorDialog(_("Failed to launch browser!"));
	}
}
