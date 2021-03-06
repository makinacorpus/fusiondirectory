<?php
/*
  This code is part of FusionDirectory (http://www.fusiondirectory.org/)
  Copyright (C) 2011-2015  FusionDirectory

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

class IconThemeDir
{
  private $Size;
  private $MinSize;
  private $MaxSize;
  private $Type      = 'Threshold';
  private $Threshold = 2;

  function __construct($infos)
  {
    $this->Size     = $infos['Size'];
    $this->MinSize  = $infos['Size'];
    $this->MaxSize  = $infos['Size'];
    foreach (array('Type', 'MaxSize', 'MinSize', 'Threshold') as $key) {
      if (isset($infos[$key])) {
        $this->$key = $infos[$key];
      }
    }
    /* Thanks to this Threshold and Scaled are the same */
    if ($this->Type == 'Threshold') {
      $this->MinSize = $this->Size - $this->Threshold;
      $this->MaxSize = $this->Size + $this->Threshold;
    }
  }

  function MatchesSize($size)
  {
    switch ($this->Type) {
      case 'Fixed':
        return ($this->Size == $size);
      case 'Threshold':
      case 'Scalable':
        return (($this->MinSize <= $size) && ($size <= $this->MaxSize));
    }
  }

  function SizeDistance($size)
  {
    switch ($this->Type) {
      case 'Fixed':
        return abs($this->Size - $size);
      case 'Threshold':
      case 'Scalable':
        if ($size < $this->MinSize) {
          return $this->MinSize - $size;
        }
        if ($size > $this->MaxSize) {
          return $size - $this->MaxSize;
        }
        return 0;
    }
  }
}

class IconTheme
{
  private $subdirs = array();
  private $path;
  private $parent;

  function __construct($folder, $default_parent)
  {
    $this->path = $folder;
    $datas  = @parse_ini_file($folder.'/index.theme', TRUE, INI_SCANNER_RAW);
    if ($datas === FALSE) {
      throw new Exception('Error while parsing theme file');
    }
    if (isset($datas['Icon Theme']['Directories']) && !empty($datas['Icon Theme']['Directories'])) {
      $dirs   = preg_split('/,/', $datas['Icon Theme']['Directories']);
      foreach ($dirs as $name) {
        $this->subdirs[strtolower($datas[$name]['Context'])][$name] = new IconThemeDir($datas[$name]);
      }
    }

    if (isset($datas['Icon Theme']['Inherits'])) {
      $this->parent = $datas['Icon Theme']['Inherits'];
    } else {
      $this->parent = $default_parent;
    }
  }

  function FindIcon($context, $icon, $size)
  {
    $context = strtolower($context);
    return $this->FindIconHelper($context, $icon, $size);
  }

  function FindIconHelper($context, $icon, $size)
  {
    $filename = $this->LookupIcon($context, $icon, $size);
    if ($filename != NULL) {
      return $filename;
    }
    if (isset(self::$fallbacks[$context.'/'.$icon])) {
      foreach (self::$fallbacks[$context.'/'.$icon] as $fallback) {
        $filename = $this->LookupIcon($fallback[0], $fallback[1], $size);
        if ($filename != NULL) {
          return $filename;
        }
      }
    }

    if ($this->parent !== NULL) {
      $parent = $this->findTheme($this->parent);
      if ($parent === NULL) {
        $parent = $this->findTheme(self::$default_theme);
      }
      return $parent->FindIconHelper($context, $icon, $size);
    }

    return NULL;
  }

  function LookupIcon($context, $iconname, $size)
  {
    foreach ($this->subdirs[$context] as $path => &$subdir) {
      if ($subdir->MatchesSize($size)) {
        foreach (self::$extensions as $extension) {
          $filename = $this->path.'/'.$path.'/'.$iconname.'.'.$extension;
          if (file_exists($filename)) {
            return $filename;
          }
        }
      }
    }
    unset($subdir);
    if (self::$find_closest) {
      $minimal_size = PHP_INT_MAX;
      foreach ($this->subdirs[$context] as $path => &$subdir) {
        if (($sizedistance = $subdir->SizeDistance($size)) < $minimal_size) {
          foreach (self::$extensions as $extension) {
            $filename = $this->path.'/'.$path.'/'.$iconname.'.'.$extension;
            if (file_exists($filename)) {
              $closest_filename = $filename;
              $minimal_size     = $sizedistance;
            }
          }
        }
      }
      unset($subdir);
      if (isset($closest_filename)) {
        return $closest_filename;
      }
    }
    return NULL;
  }

  static public $default_theme  = 'default';
  static public $extensions     = array('png', 'xpm', 'svg');
  static public $find_closest   = FALSE;

  /* We store themes in the session. To do otherwise, override these methods. */
  static public $session_var    = 'IconThemes';
  static public function loadThemes($path)
  {
    $themes = array();
    if ($dir = opendir("$path")) {
      while (($file = readdir($dir)) !== FALSE) {
        if (file_exists("$path/$file/index.theme") && !preg_match("/^\./", $file)) {
          try {
            if ($file == self::$default_theme) {
              $themes[$file] = new IconTheme("$path/$file",  NULL);
            } else {
              $themes[$file] = new IconTheme("$path/$file", self::$default_theme);
            }
          } catch (Exception $e) {
          }
        }
      }
    }
    $_SESSION[self::$session_var] = $themes;
  }
  static public function findThemeIcon($theme, $context, $icon, $size)
  {
    if (!isset($_SESSION[self::$session_var])) {
      die('Error: no theme found in session');
    }
    if (isset($_SESSION[self::$session_var][$theme])) {
      return $_SESSION[self::$session_var][$theme]->FindIcon($context, $icon, $size);
    }
    return $_SESSION[self::$session_var][self::$default_theme]->FindIcon($context, $icon, $size);
  }
  public function findTheme($theme)
  {
    if (isset($_SESSION[self::$session_var][$theme])) {
      $ret = &$_SESSION[self::$session_var][$theme];
      return $ret;
    }
    return NULL;
  }

  /* Fallback system */
  static public $fallbacks = array(
    'types/user-group'              => array(
      array('applications','system-users')
    ),
    'types/resource-group'          => array(
      array('actions','resource-group')
    ),
    'types/user'                    => array(
      array('places','user-identity'),
      array('status','avatar-default'),
    ),
    'types/contact'                 => array(
      array('mimetypes','x-office-contact'),
    ),
    'types/certificate'             => array(
      array('mimetypes','stock_certificate'),
      array('mimetypes','application-certificate'),
      array('actions','view-certificate'),
    ),
    'applications/user-info'        => array(
      array('actions','user-properties'),
      array('types','contact'),
      array('mimetypes','x-office-contact'),
      array('types','user'),
      array('places','user-identity'),
      array('status','avatar-default'),
    ),
    'applications/office-calendar'  => array(
      array('mimetypes','x-office-calendar'),
    ),
    'applications/os-linux'         => array(
      array('applications','linux'),
    ),
    'applications/os-windows'       => array(
      array('applications','windows'),
    ),
    'applications/samba'            => array(
      array('applications','os-windows'),
      array('applications','windows'),
    ),
    'applications/config-language'  => array(
      array('applications','locale'),
      array('applications','preferences-desktop-locale'),
    ),
    'mimetypes/text-csv'            => array(
      array('mimetypes','x-office-spreadsheet'),
      array('mimetypes','text-x-generic'),
    ),
    'mimetypes/application-pdf'     => array(
      array('mimetypes','x-office-document'),
    ),
    'actions/application-exit'      => array(
      array('actions','system-log-out'),
    ),
    'actions/document-export'       => array(
      array('actions','document-send'),
    ),
    'actions/download'              => array(
      array('actions','document-save'),
    ),
    'actions/document-restore'      => array(
      array('actions','document-import'),
      array('actions','document-open'),
    ),
    'actions/document-edit'         => array(
      array('actions','edit'),
      array('applications','text-editor'),
      array('applications','accessories-text-editor'),
      array('actions','document-open'),
    ),
    'actions/snapshot'              => array(
      array('actions','document-save'),
    ),
    'actions/system-reboot'         => array(
      array('actions','view-refresh'),
    ),
    'actions/system-update'         => array(
      array('applications','system-software-update'),
    ),
    'actions/system-reinstall'      => array(
      array('applications','system-installer'),
    ),
    'actions/up'                    => array(
      array('actions','go-up'),
      array('actions','arrow-up'),
    ),
    'actions/down'                  => array(
      array('actions','go-down'),
      array('actions','arrow-down'),
    ),
    'actions/previous'              => array(
      array('actions','go-previous'),
      array('actions','arrow-left'),
    ),
    'actions/next'                  => array(
      array('actions','go-next'),
      array('actions','arrow-right'),
    ),
    'actions/submit'                => array(
      array('actions','go-jump'),
    ),
    'categories/settings'           => array(
      array('categories','gnome-settings'),
      array('categories','preferences-other'),
      array('categories','preferences-system'),
    ),
    'categories/checks'             => array(
      array('actions','view-task'),
      array('actions','view-calendar-tasks'),
      array('actions','checkbox'),
      array('status','task-complete'),
    ),
    'devices/server'                => array(
      array('places','server'),
      array('places','network-server'),
    ),
    'devices/media-cdrom'           => array(
      array('devices','media-optical'),
    ),
    'devices/terminal'              => array(
      array('applications','utilities-terminal'),
    ),
    'devices/computer-windows'      => array(
      array('applications','os-windows'),
      array('applications','windows'),
    ),
    'status/object-locked'          => array(
      array('status','changes-prevent'),
    ),
    'status/object-unlocked'        => array(
      array('status','changes-allow'),
    ),
  );
}

?>
