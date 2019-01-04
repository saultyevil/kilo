/* ***************************************************************************
 *
 * @file io.c
 *
 * @date 01/01/2019
 *
 * @author E. J. Parkinson
 *
 * @brief
 *
 * @details
 *
 * ************************************************************************** */


#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "kris.h"


// Display a prompt in the status bar, and let the user input a line of text
char *status_bar_prompt (char *prompt_msg, void (*callback)(char *, int))
{
  int c;
  size_t buf_len, buf_size;
  char *buf;

  buf_len = 0;
  buf_size = 128;
  buf = malloc (buf_size);
  buf[0] = '\0';

  // Keep looping until buf can be returned
  while (TRUE)
  {
    set_status_message (prompt_msg, buf);
    refresh_editor_screen ();

    c = read_keypress ();

    // Allow a user to delete something in the status prompt
    if (c == DEL_KEY || c == CTRL_KEY ('h') || c == BACK_SPACE)
    {
      if (buf_len != 0)
        buf[--buf_len] = '\0';
    }
    // Cancel and return NULL if escape is pressed
    else if (c == '\x1b')
    {
      set_status_message ("");
      if (callback)
        callback (buf, c);
      free (buf);
      return NULL;
    }
    // If enter is pressed
    else if (c == '\r')
    {
      if (buf_len != 0)
      {
        set_status_message ("");
        if (callback)
          callback (buf, c);
        return buf;
      }
    }
    // Else if not a control sequence keep adding chars to buf
    else if (!iscntrl (c) && c < 128)
    {
      if (buf_len == buf_size - 1)
      {
        buf_size *= 2;
        buf = realloc (buf, buf_size);
      }

      buf[buf_len++] = (char) c;
      buf[buf_len] = '\0';
    }

    if (callback)
      callback (buf, c);
  }
}

// Open a file and red into the text buffer
int open_file (char *filename)
{
  char *line;
  size_t line_cap;
  ssize_t line_len;
  FILE *input_file;

  free (editor.filename);
  // strdup duplicates a string for a new pointer
  editor.filename = strdup (filename);

  if (!(input_file = fopen (filename, "r")))
  {
    set_status_message ("Couldn't open file %s: %s", editor.filename,
                         strerror (errno));
    editor.filename = NULL;
    // Set errno to 0, otherwise the main loop will exit
    errno = 0;
    return FALSE;
  }

  select_syntax_highlighting ();

  // Read in EACH line of the input file and append to the text buffer
  line = NULL;
  line_cap = 0;
  while ((line_len = getline (&line, &line_cap, input_file) != -1))
  {
    // TODO: replace this strlen with working version of getline return value
    line_len = strlen (line);
    // Strip off the return or new line chars and append to the text buffer
    while (line_len > 0 &&
                     (line[line_len - 1] == '\n' || line[line_len - 1] == '\r'))
      line_len--;
    append_line_to_text_buffer (editor.nlines, line, (size_t) line_len);
  }

  free (line);
  if (fclose (input_file))
    error ("Couldn't close input file");
  editor.modified = FALSE;

  return TRUE;
}

// Convert array of eline data types into a single string
char *rows_to_strings (size_t *buf_len)
{
  char *buf, *p;
  size_t i, tot_len;

  // Figure out the total number of chars in the text buffer
  tot_len = 0;
  for (i = 0; i < editor.nlines; i++)
    tot_len += editor.lines[i].len + 1;
  *buf_len = tot_len;

  // Copy the contents of each row to the end of the buffer and append a new
  // line character at the end of each row
  p = buf = malloc (tot_len);
  for (i = 0; i < editor.nlines; i++)
  {
    memcpy (p, editor.lines[i].chars, editor.lines[i].len);
    p += editor.lines[i].len;
    *p = '\n';
    p++;
  }

  return buf;
}

// Save the text buffer to file
void save_file (void)
{
  size_t buf_len;
  char *buf;
  int file_desc;

  // Prompt the user for a filename
  if (editor.filename == NULL)
  {
    editor.filename = status_bar_prompt ("Save as: %s", NULL);
    if (editor.filename == NULL)
    {
      set_status_message ("Save aborted");
      return;
    }

    select_syntax_highlighting ();
  }

  // Retrieve the text buffer in the form of strings, create the file in 0644
  // mode and then write to file
  buf = rows_to_strings (&buf_len);
  if (((file_desc = open (editor.filename, O_RDWR | O_CREAT, 0644)) != -1))
  {
    if (ftruncate (file_desc, buf_len) != -1)
    {
      if (write (file_desc, buf, buf_len) == buf_len)
      {
        close (file_desc);
        free (buf);
        editor.modified = FALSE;
        set_status_message ("%d bytes written to disk", buf_len);
        return;
      }
    }

    close (file_desc);
  }

  free (buf);
  set_status_message ("Can't save file. I/O error: %s", strerror (errno));
}

// Append a string to the screen buffer
void append_to_screen_buf (SCREEN_BUF *sb, char *s, size_t len)
{
  char *new;
  if (!(new = realloc (sb->buf, sb->len + len)))
    error ("Couldn't allocate memory for screen buffer");

  // Copy string s onto the end of the screen buffer and update the buffer
  // pointer in the SCREEN_BUF struct
  memcpy (&new[sb->len], s, len);
  sb->buf = new;
  sb->len += len;
}

// Free the screen buffer
void free_screen_buf (SCREEN_BUF *sb)
{
  free (sb->buf);
}
