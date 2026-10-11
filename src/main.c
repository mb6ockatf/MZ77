#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <time.h>
#include "antidebugger.h"
#include "dt.h"
#include "queue.h"
#include "lz77.h"
#include "messages.h"
#include <getopt.h>


static gboolean
ui_update (gpointer user_data)
{
  AppData *app = user_data;
  int total = atomic_load (&app->total);
  int done = atomic_load (&app->completed);
  double fraction = (total > 0) ? (double) done / total : 0.0;
  char text[32];
  snprintf (text, sizeof (text), "%d / %d", done, total);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (app->progress), fraction);
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (app->progress), text);
  return G_SOURCE_REMOVE;
}

static void
set_state (AppData *app, gboolean run, const char *text)
{
  atomic_store (&app->busy, run);
  gtk_widget_set_sensitive (app->start_btn, !run);
  gtk_label_set_text (GTK_LABEL (app->status), text);
}

static gboolean
apply_state (gpointer user_data)
{
  StateData *state = user_data;
  set_state (state->app, state->run, state->msg);
  if (!state->run && atomic_load (&state->app->pending) > 0)
    {
      atomic_store (&state->app->total, atomic_load (&state->app->pending));
      atomic_store (&state->app->completed, 0);
    }
  ui_update (state->app);
  g_free (state);
  return G_SOURCE_REMOVE;
}

static void *
worker (void *user_data)
{
  AppData *app = user_data;
  int total = atomic_load (&app->total);
  int failed = 0;
  int processed = 0;
  TaskItem task;
  clock_t begin = clock ();
  for (int task_index = 0; task_index < total; task_index++)
    {
      if (atomic_load (&app->queue.shutdown))
	break;
      if (!queue_pop (&app->queue, &task))
	break;
      atomic_fetch_sub (&app->pending, 1);
      printf ("Total: %d; Got a task\n", total);
      task.status = STATUS_RUNNING;
      int result;
      if (task.mode == MODE_COMPRESS)
	result =
	  lz77_compress (task.data.paths.in_path, task.data.paths.out_path);
      else
	result =
	  lz77_decompress (task.data.paths.in_path, task.data.paths.out_path);
      task.status = (result == EXIT_SUCCESS) ? STATUS_DONE : STATUS_ERROR;
      if (task.status == STATUS_ERROR)
	{
	  failed++;
	  fprintf (stderr, "mz77: failed: %s -> %s\n",
		   task.data.paths.in_path, task.data.paths.out_path);
	}
      processed++;
      atomic_fetch_add (&app->completed, 1);
      g_idle_add (ui_update, app);
    }
  clock_t end = clock ();
  printf ("all done at %f\n", (double) (end - begin));
  StateData *state = g_new0 (StateData, 1);
  state->app = app;
  state->run = FALSE;
  if (failed > 0)
    snprintf (state->msg, sizeof (state->msg), "Done, %d of %d failed",
	      failed, processed);
  else
    g_strlcpy (state->msg, "Done", sizeof (state->msg));
  g_idle_add (apply_state, state);
  return NULL;
}

static void
on_picked_multiple (GObject *source, GAsyncResult *result, gpointer user_data)
{
  AppData *app = (AppData *) user_data;
  GError *error = NULL;
  GListModel *files =
    gtk_file_dialog_open_multiple_finish (GTK_FILE_DIALOG (source), result,
					  &error);
  if (!files || error)
    {
      g_clear_error (&error);
      if (files)
	g_object_unref (files);
      return;
    }
  gboolean is_comp = gtk_switch_get_active (GTK_SWITCH (app->mode_switch));
  enum Mode mode = is_comp ? MODE_COMPRESS : MODE_DECOMPRESS;
  guint count = g_list_model_get_n_items (files);
  int added = 0;
  for (guint file_index = 0; file_index < count; file_index++)
    {
      GFile *file = G_FILE (g_list_model_get_item (files, file_index));
      char *path = g_file_get_path (file);
      if (path)
	{
	  TaskItem task = {.mode = mode,.status = STATUS_PENDING };
	  g_strlcpy (task.data.paths.in_path, path, PATH_MAX);
	  char *base = g_path_get_basename (path);
	  const char *target_dir =
	    (app->out_dir[0] != '\0') ? app->out_dir : g_get_home_dir ();
	  char *tmp_path = g_build_filename (target_dir, base, NULL);
	  g_strlcpy (task.data.paths.out_path, tmp_path, PATH_MAX);
	  g_free (tmp_path);
	  if (mode == MODE_COMPRESS)
	    g_strlcat (task.data.paths.out_path, ".lz77", PATH_MAX);
	  else
	    {
	      size_t len = strlen (task.data.paths.out_path);
	      if (len > 5
		  && strcmp (task.data.paths.out_path + len - 5,
			     ".lz77") == 0)
		task.data.paths.out_path[len - 5] = '\0';
	    }
	  g_free (base);
	  queue_push (&app->queue, &task);
	  added++;
	  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	  gtk_box_append (GTK_BOX (row), gtk_label_new (path));
	  gtk_box_append (GTK_BOX (row), gtk_label_new ("Wait"));
	  gtk_list_box_insert (GTK_LIST_BOX (app->list_box), row, -1);
	  gtk_widget_set_visible (row, TRUE);
	  g_free (path);
	}
      g_object_unref (file);
    }
  atomic_fetch_add (&app->pending, added);
  if (!atomic_load (&app->busy))
    {
      atomic_store (&app->total, atomic_load (&app->pending));
      atomic_store (&app->completed, 0);
      ui_update (app);
    }
  g_object_unref (files);
}

static void
on_picked_output_dir (GObject *src, GAsyncResult *res, gpointer user_data)
{
  AppData *app = (AppData *) user_data;
  GError *err = NULL;
  GFile *folder =
    gtk_file_dialog_select_folder_finish (GTK_FILE_DIALOG (src), res, &err);

  if (!folder || err)
    {
      g_clear_error (&err);
      return;
    }

  char *path = g_file_get_path (folder);
  if (path)
    {
      g_strlcpy (app->out_dir, path, PATH_MAX);
      gtk_label_set_text (GTK_LABEL (app->out_dir_label), path);
      g_free (path);
    }
  g_object_unref (folder);
}

static void
on_select_output_dir (GtkButton *btn, AppData *app)
{
  (void) btn;
  GtkWindow *win = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (btn)));
  GtkFileDialog *dlg = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dlg, "Select Output Directory");
  gtk_file_dialog_select_folder (dlg, win, NULL, on_picked_output_dir, app);
}

static void
on_add_files (GtkButton *btn, AppData *app)
{
  (void) btn;
  GtkWindow *win = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (btn)));
  GtkFileDialog *dlg = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dlg, "Select files");
  gtk_file_dialog_open_multiple (dlg, win, NULL, on_picked_multiple, app);
}

static void
on_mode_toggle (GtkSwitch *sw, gboolean state, AppData *app)
{
  (void) sw;
  gtk_label_set_text (GTK_LABEL (app->mode_label),
		      state ? "Mode: compress" : "Mode: decompress");
}

static gboolean
on_close (GtkWindow *window, AppData *app)
{
  (void) window;
  queue_shutdown (&app->queue);
  g_main_loop_quit ((GMainLoop *) app->loop);
  return FALSE;
}

static void
on_start (GtkButton *button, AppData *app)
{
  (void) button;
  if (atomic_load (&app->busy))
    return;
  if (atomic_load (&app->pending) == 0)
    {
      set_state (app, FALSE, "Add files to queue");
      return;
    }
  if (app->has_worker)
    {
      pthread_join (app->worker, NULL);
      app->has_worker = false;
    }
  atomic_store (&app->total, atomic_load (&app->pending));
  atomic_store (&app->completed, 0);
  ui_update (app);
  set_state (app, TRUE, "Working...");
  if (pthread_create (&app->worker, NULL, worker, app) != 0)
    {
      set_state (app, FALSE, "Failed to start worker");
      return;
    }
  app->has_worker = true;
}

static void
parse_options (int argc, char **argv)
{
  static const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0}
  };
  int option;
  while ((option = getopt_long (argc, argv, "hV", long_options, NULL)) != -1)
    {
      switch (option)
	{
	case 'h':
	  show_help (argv[0]);
	case 'V':
	  show_version ();
	default:
	  fprintf (stderr, "Try '%s --help' for more information.\n",
		   argv[0]);
	  exit (EXIT_FAILURE);
	}
    }
  if (optind < argc)
    {
      fprintf (stderr, "%s: unexpected argument '%s'\n", argv[0],
	       argv[optind]);
      fprintf (stderr, "Try '%s --help' for more information.\n", argv[0]);
      exit (EXIT_FAILURE);
    }
}

int
main (int argc, char **argv)
{
  parse_options (argc, argv);
  check_debugger ();
  gtk_init ();
  AppData app = { 0 };
  app.loop = g_main_loop_new (NULL, FALSE);
  g_strlcpy (app.out_dir, g_get_home_dir (), PATH_MAX);
  queue_init (&app.queue);
  GtkWidget *win = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (win), "MZ77 Archiver");
  gtk_window_set_default_size (GTK_WINDOW (win), 440, 380);
  g_signal_connect (win, "close-request", G_CALLBACK (on_close), &app);
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_margin_start (box, 15);
  gtk_widget_set_margin_end (box, 15);
  gtk_widget_set_margin_top (box, 15);
  gtk_widget_set_margin_bottom (box, 15);
  gtk_window_set_child (GTK_WINDOW (win), box);
  app.list_box = gtk_list_box_new ();
  gtk_widget_set_vexpand (app.list_box, TRUE);
  gtk_box_append (GTK_BOX (box), app.list_box);
  app.mode_label = gtk_label_new ("Mode: decompress");
  app.mode_switch = gtk_switch_new ();
  GtkWidget *mr = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append (GTK_BOX (mr), app.mode_label);
  gtk_box_append (GTK_BOX (mr), app.mode_switch);
  gtk_widget_set_halign (app.mode_label, GTK_ALIGN_START);
  g_signal_connect (app.mode_switch, "state-set", G_CALLBACK (on_mode_toggle),
		    &app);
  GtkWidget *add_btn = gtk_button_new_with_label ("Add files");
  g_signal_connect (add_btn, "clicked", G_CALLBACK (on_add_files), &app);
  app.progress = gtk_progress_bar_new ();
  app.status = gtk_label_new ("Waiting for files");
  app.start_btn = gtk_button_new_with_label ("Start");
  app.out_dir_label = gtk_label_new (app.out_dir);
  app.out_dir_btn = gtk_button_new_with_label ("Set Output Directory");
  g_signal_connect (app.out_dir_btn, "clicked",
		    G_CALLBACK (on_select_output_dir), &app);

  GtkWidget *dir_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append (GTK_BOX (dir_box), gtk_label_new ("Save to:"));
  gtk_box_append (GTK_BOX (dir_box), app.out_dir_label);
  gtk_box_append (GTK_BOX (dir_box), app.out_dir_btn);

  gtk_label_set_ellipsize (GTK_LABEL (app.out_dir_label),
			   PANGO_ELLIPSIZE_MIDDLE);
  gtk_widget_set_hexpand (app.out_dir_label, TRUE);
  g_signal_connect (app.start_btn, "clicked", G_CALLBACK (on_start), &app);
  gtk_box_append (GTK_BOX (box), mr);
  gtk_box_append (GTK_BOX (box), add_btn);
  gtk_box_append (GTK_BOX (box), app.progress);
  gtk_box_append (GTK_BOX (box), app.status);
  gtk_box_append (GTK_BOX (box), app.start_btn);
  gtk_box_insert_child_after (GTK_BOX (box), dir_box, app.list_box);
  gtk_window_present (GTK_WINDOW (win));
  g_main_loop_run (app.loop);
  queue_shutdown (&app.queue);
  if (app.has_worker)
    pthread_join (app.worker, NULL);
  g_main_loop_unref (app.loop);
  queue_destroy (&app.queue);
  return EXIT_SUCCESS;
}
