/*
 * execli.c
 * $Id: execli.c,v 1.2 2005/12/08 20:42:39 bobi Exp $
 *
 * Copyright 2004 Bobi B., w1zard0f07@yahoo.com
 *
 * This file is part of hdl_dump.
 *
 * hdl_dump is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * hdl_dump is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hdl_dump; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <windows.h>
#include <stdio.h>


/* reads up to `bytes' from the `pipe' and returns the number of bytes get */
int
non_blocking_pipe_read (HANDLE pipe,
			char *buffer,
			size_t bytes)
{
  size_t bytes_orig = bytes;
  DWORD bytes_left;
  BOOL success;

  do
    { /* ask how much bytes could be get from the pipe */
      bytes_left = 0;
      success = PeekNamedPipe (pipe, NULL, 0, NULL, &bytes_left, NULL);
      if (success && bytes_left > 0)
	{
	  DWORD bytes_readen;
	  if (bytes_left > bytes)
	    bytes_left = bytes;
	  success = ReadFile (pipe, buffer, bytes_left, &bytes_readen, NULL);
	  if (success)
	    {
	      bytes -= bytes_readen;
	      buffer += bytes_readen;
	    }
	}
    }
  while (success && bytes_left > 0 && bytes > 0);
  return (bytes_orig - bytes);
}


/* executes a command-line interface program and passes all output
   via `message_callback'; upon complete returns exit code */
__stdcall int
exec_cli (const char *file_path,
	  const char *cmd_line,
	  const char *current_dir,
	  __stdcall int (*message_callback) (const char *output,
					     size_t length))
{
#define READ_BUFFER_SIZE 1024

  HANDLE out_read = NULL, out_write = NULL;
  SECURITY_ATTRIBUTES sa;
  BOOL success;
  char command_line [512];
  DWORD exit_code = (DWORD) -1;

  memset (&sa, 0, sizeof (sa));
  sa.nLength = sizeof (sa);
  sa.bInheritHandle = TRUE;
  success = CreatePipe (&out_read, &out_write, &sa, 0);
  if (success)
    {
      STARTUPINFOA si;
      PROCESS_INFORMATION pi;

      memset (&si, 0, sizeof (si));
      si.cb = sizeof (si);
      si.hStdInput = INVALID_HANDLE_VALUE;
      si.hStdOutput = out_write;
      si.hStdError = out_write;
      si.dwFlags = STARTF_USESTDHANDLES;

      memset (&pi, 0, sizeof (pi));

      sprintf (command_line, "\"%s\" %s", file_path, cmd_line);
      success = CreateProcess (file_path, command_line, NULL, NULL,
			       TRUE, CREATE_NO_WINDOW, NULL, current_dir, &si, &pi);
      if (success)
	{ /* wait 'till process is over and nothing left to read from stdout */
	  char buffer [READ_BUFFER_SIZE];
	  size_t bytes;
	  BOOL process_alive;
	  do
	    {
	      Sleep (10); /* fixes 100% CPU usage issue */
	      process_alive = (GetExitCodeProcess (pi.hProcess, &exit_code) &&
			       exit_code == STILL_ACTIVE);
	      do
		{ /* pipe all data to the caller */
		  bytes = non_blocking_pipe_read (out_read, buffer,
						  sizeof (buffer) /
						  sizeof (buffer [0]));
		  if (bytes > 0)
		    (*message_callback) (buffer, bytes);
		}
	      while (bytes > 0);
	    }
	  while (process_alive || bytes > 0);
	}
      CloseHandle (out_read);
      CloseHandle (out_write);
    }
  return ((int) exit_code);
}


#if defined (_CONSOLE_TEST)

__stdcall int
callback (const char *output,
	  size_t length)
{
  printf ("%d: [%.*s]", length, length, output);
  return (0);
}


int
main (int argc, char *argv [])
{
  printf ("exit code: %d",
	  exec_cli ("./hdl_dump.exe",
		    "cdvd_info i:/KALIFORNIA.ISO",
		    "t:/p/ps2/hdl_dump/",
		    &callback));
  return (0);
}

#endif /* _CONSOLE_TEST defined? */
