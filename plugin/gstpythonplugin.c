/* gst-python
 * Copyright (C) 2009 Edward Hervey <bilboed@bilboed.com>
 *               2005 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* include this first, before NO_IMPORT_PYGOBJECT is defined */
#include <pygobject.h>
#include <gst/gst.h>
#include <gmodule.h>
#include <Python.h>

void *_PyGstElement_Type;

GST_DEBUG_CATEGORY_STATIC (pyplugindebug);
#define GST_CAT_DEFAULT pyplugindebug

#define GST_ORIGIN "http://gstreamer.freedesktop.org"

static PyObject *element;

static gboolean
gst_python_plugin_load_file (GstPlugin * plugin, const char *name)
{
  PyObject *main_module, *main_locals;
  PyObject *elementfactory;
  PyObject *module;
  const gchar *facname;
  guint rank;
  PyObject *class;

  GST_DEBUG ("loading plugin %s", name);

  main_module = PyImport_AddModule ("__main__");
  if (main_module == NULL) {
    GST_WARNING ("Could not get __main__, ignoring plugin %s", name);
    PyErr_Print ();
    PyErr_Clear ();
    return FALSE;
  }

  main_locals = PyModule_GetDict (main_module);
  module =
      PyImport_ImportModuleEx ((char *) name, main_locals, main_locals, NULL);
  if (!module) {
    GST_DEBUG ("Could not load module, ignoring plugin %s", name);
    PyErr_Print ();
    PyErr_Clear ();
    return FALSE;
  }

  /* Get __gstelementfactory__ from file */
  elementfactory = PyObject_GetAttrString (module, "__gstelementfactory__");
  if (!elementfactory) {
    GST_DEBUG ("python file doesn't contain __gstelementfactory__");
    PyErr_Clear ();
    return FALSE;
  }

  /* parse tuple : name, rank, gst.ElementClass */
  if (!PyArg_ParseTuple (elementfactory, "sIO", &facname, &rank, &class)) {
    GST_WARNING ("__gstelementfactory__ isn't correctly formatted");
    PyErr_Print ();
    PyErr_Clear ();
    Py_DECREF (elementfactory);
    return FALSE;
  }

  if (!PyObject_IsSubclass (class, (PyObject *) & PyGObject_Type)) {
    GST_WARNING ("the class provided isn't a subclass of GObject.Object");
    PyErr_Print ();
    PyErr_Clear ();
    Py_DECREF (elementfactory);
    Py_DECREF (class);
    return FALSE;
  }

  if (!g_type_is_a (pyg_type_from_object (class), GST_TYPE_ELEMENT)) {
    GST_WARNING ("the class provided isn't a subclass of Gst.Element");
    PyErr_Print ();
    PyErr_Clear ();
    Py_DECREF (elementfactory);
    Py_DECREF (class);
    return FALSE;
  }

  GST_INFO ("Valid plugin");
  Py_DECREF (elementfactory);

  return gst_element_register (plugin, facname, rank,
      pyg_type_from_object (class));
}

static gboolean
gst_python_load_directory (GstPlugin * plugin, gchar * path)
{
  GDir *dir;
  const gchar *file;
  GError *error = NULL;
  gboolean ret = TRUE;

  dir = g_dir_open (path, 0, &error);
  if (!dir) {
    /*retval should probably be depending on error, but since we ignore it... */
    GST_DEBUG ("Couldn't open Python plugin dir: %s", error->message);
    g_error_free (error);
    return FALSE;
  }
  while ((file = g_dir_read_name (dir))) {
    /* FIXME : go down in subdirectories */
    if (g_str_has_suffix (file, ".py")) {
      gsize len = strlen (file) - 3;
      gchar *name = g_strndup (file, len);
      ret &= gst_python_plugin_load_file (plugin, name);
      g_free (name);
    }
  }
  return TRUE;
}

static gboolean
gst_python_plugin_load (GstPlugin * plugin)
{
  PyObject *sys_path;
  const gchar *plugin_path;
  gboolean ret = TRUE;

  sys_path = PySys_GetObject ("path");

  /* Mimic the order in which the registry is checked in core */

  /* 1. check env_variable GST_PLUGIN_PATH */
  plugin_path = g_getenv ("GST_PLUGIN_PATH");
  if (plugin_path) {
    char **list;
    int i;

    GST_DEBUG ("GST_PLUGIN_PATH set to %s", plugin_path);
    list = g_strsplit (plugin_path, G_SEARCHPATH_SEPARATOR_S, 0);
    for (i = 0; list[i]; i++) {
      gchar *sysdir = g_build_filename (list[i], "python", NULL);
      PyList_Insert (sys_path, 0, PyUnicode_FromString (sysdir));
      gst_python_load_directory (plugin, sysdir);
      g_free (sysdir);
    }

    g_strfreev (list);
  }

  /* 2. Check for GST_PLUGIN_SYSTEM_PATH */
  plugin_path = g_getenv ("GST_PLUGIN_SYSTEM_PATH");
  if (plugin_path == NULL) {
    char *home_plugins;

    /* 2.a. Scan user and system-wide plugin directory */
    GST_DEBUG ("GST_PLUGIN_SYSTEM_PATH not set");

    /* plugins in the user's home directory take precedence over
     * system-installed ones */
    home_plugins = g_build_filename (g_get_home_dir (),
        ".gstreamer-" GST_API_VERSION, "plugins", "python", NULL);
    PyList_Insert (sys_path, 0, PyUnicode_FromString (home_plugins));
    gst_python_load_directory (plugin, home_plugins);
    g_free (home_plugins);

    /* add the main (installed) library path */
    PyList_Insert (sys_path, 0, PyUnicode_FromString (PLUGINDIR "/python"));
    gst_python_load_directory (plugin, PLUGINDIR "/python");
  } else {
    gchar **list;
    gint i;

    /* 2.b. Scan GST_PLUGIN_SYSTEM_PATH */
    GST_DEBUG ("GST_PLUGIN_SYSTEM_PATH set to %s", plugin_path);
    list = g_strsplit (plugin_path, G_SEARCHPATH_SEPARATOR_S, 0);
    for (i = 0; list[i]; i++) {
      gchar *sysdir;

      sysdir = g_build_filename (list[i], "python", NULL);

      PyList_Insert (sys_path, 0, PyUnicode_FromString (sysdir));
      gst_python_load_directory (plugin, sysdir);
      g_free (sysdir);
    }
    g_strfreev (list);
  }


  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  PyGILState_STATE state;
  PyObject *gst, *dict, *pyplugin;
  gboolean we_initialized = FALSE;
  GModule *libpython;
  gpointer has_python = NULL;
  PyObject *seq, *list;
  int i, len;

  GST_DEBUG_CATEGORY_INIT (pyplugindebug, "pyplugin", 0,
      "Python plugin loader");

  gst_plugin_add_dependency_simple (plugin,
      "HOME/.gstreamer-" GST_API_VERSION
      "/plugins/python:GST_PLUGIN_SYSTEM_PATH/python:GST_PLUGIN_PATH/python",
      PLUGINDIR "/python:HOME/.gstreamer-" GST_API_VERSION "/plugins/python:"
      "GST_PLUGIN_SYSTEM_PATH/python:GST_PLUGIN_PATH/python", NULL,
      GST_PLUGIN_DEPENDENCY_FLAG_NONE);

  GST_LOG ("Checking to see if libpython is already loaded");
  g_module_symbol (g_module_open (NULL, G_MODULE_BIND_LOCAL), "_Py_NoneStruct",
      &has_python);
  if (has_python) {
    GST_LOG ("libpython is already loaded");
  } else {
    GST_LOG ("loading libpython");
    libpython =
        g_module_open (PY_LIB_LOC "/libpython" PYTHON_VERSION PY_ABI_FLAGS
        "." PY_LIB_SUFFIX, 0);
    if (!libpython) {
      g_critical ("Couldn't g_module_open libpython. Reason: %s",
          g_module_error ());
      return FALSE;
    }
  }

  if (!Py_IsInitialized ()) {
    GST_LOG ("python wasn't initialized");
    /* set the correct plugin for registering stuff */
    Py_Initialize ();
    we_initialized = TRUE;
  } else {
    GST_LOG ("python was already initialized");
    state = PyGILState_Ensure ();
  }

  GST_LOG ("initializing pygobject");
  if (!pygobject_init (3, 0, 0)) {
    g_critical ("pygobject initialization failed");
    return FALSE;
  }

  gst = PyImport_ImportModule ("gi.repository.Gst");
  if (!gst) {
    g_critical ("can't find gi.repository.Gst");
    return FALSE;
  }

  if (we_initialized) {
    PyObject *tmp;

    dict = PyModule_GetDict (gst);
    if (!dict) {
      g_critical ("gi.repository.Gst is no dict");
      return FALSE;
    }

    tmp =
        PyObject_GetAttr (PyMapping_GetItemString (dict,
            "_introspection_module"), PyUnicode_FromString ("__dict__"));

    _PyGstElement_Type = PyMapping_GetItemString (tmp, "Element");

    if (!_PyGstElement_Type) {
      g_critical ("Could not get Gst.Element");
      return FALSE;
    }

    pyplugin = pygobject_new (G_OBJECT (plugin));
    if (!pyplugin || PyModule_AddObject (gst, "__plugin__", pyplugin) != 0) {
      g_critical ("Couldn't set __plugin__ attribute");
      Py_DECREF (pyplugin);
      return FALSE;
    }
  }

  gst_python_plugin_load (plugin);

  if (we_initialized) {
    /* We need to release the GIL since we're going back to C land */
    PyEval_SaveThread ();
  } else
    PyGILState_Release (state);
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR, python,
    "loader for plugins written in python",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_ORIGIN)
