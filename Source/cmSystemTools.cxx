/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmSystemTools.h"   
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include <cmsys/RegularExpression.hxx>
#include <cmsys/Directory.hxx>
#include <cmsys/Process.h>

// support for realpath call
#ifndef _WIN32
#include <limits.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/wait.h>
#endif

#if defined(_WIN32) && (defined(_MSC_VER) || defined(__BORLANDC__))
#include <string.h>
#include <windows.h>
#include <direct.h>
#define _unlink unlink
#else
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif


bool cmSystemTools::s_RunCommandHideConsole = false;
bool cmSystemTools::s_DisableRunCommandOutput = false;
bool cmSystemTools::s_ErrorOccured = false;
bool cmSystemTools::s_FatalErrorOccured = false;
bool cmSystemTools::s_DisableMessages = false;

std::string cmSystemTools::s_Windows9xComspecSubstitute = "command.com";
void cmSystemTools::SetWindows9xComspecSubstitute(const char* str)
{
  if ( str )
    {
    cmSystemTools::s_Windows9xComspecSubstitute = str;
    }
}
const char* cmSystemTools::GetWindows9xComspecSubstitute()
{
  return cmSystemTools::s_Windows9xComspecSubstitute.c_str();
}

void (*cmSystemTools::s_ErrorCallback)(const char*, const char*, bool&, void*);
void* cmSystemTools::s_ErrorCallbackClientData = 0;

// replace replace with with as many times as it shows up in source.
// write the result into source.
#if defined(_WIN32) && !defined(__CYGWIN__)
void cmSystemTools::ExpandRegistryValues(std::string& source)
{
  // Regular expression to match anything inside [...] that begins in HKEY.
  // Note that there is a special rule for regular expressions to match a
  // close square-bracket inside a list delimited by square brackets.
  // The "[^]]" part of this expression will match any character except
  // a close square-bracket.  The ']' character must be the first in the
  // list of characters inside the [^...] block of the expression.
  cmsys::RegularExpression regEntry("\\[(HKEY[^]]*)\\]");
  
  // check for black line or comment
  while (regEntry.find(source))
    {
    // the arguments are the second match
    std::string key = regEntry.match(1);
    std::string val;
    if (ReadRegistryValue(key.c_str(), val))
      {
      std::string reg = "[";
      reg += key + "]";
      cmSystemTools::ReplaceString(source, reg.c_str(), val.c_str());
      }
    else
      {
      std::string reg = "[";
      reg += key + "]";
      cmSystemTools::ReplaceString(source, reg.c_str(), "/registry");
      }
    }
}
#else
void cmSystemTools::ExpandRegistryValues(std::string&)
{
}
#endif

std::string cmSystemTools::EscapeQuotes(const char* str)
{
  std::string result = "";
  for(const char* ch = str; *ch != '\0'; ++ch)
    {
    if(*ch == '"')
      {
      result += '\\';
      }
    result += *ch;
    }
  return result;
}

std::string cmSystemTools::EscapeSpaces(const char* str)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  std::string result;
  
  // if there are spaces
  std::string temp = str;
  if (temp.find(" ") != std::string::npos && 
      temp.find("\"")==std::string::npos)
    {
    result = "\"";
    result += str;
    result += "\"";
    return result;
    }
  return str;
#else
  std::string result = "";
  for(const char* ch = str; *ch != '\0'; ++ch)
    {
    if(*ch == ' ')
      {
      result += '\\';
      }
    result += *ch;
    }
  return result;
#endif
}

std::string cmSystemTools::RemoveEscapes(const char* s)
{
  std::string result = "";
  for(const char* ch = s; *ch; ++ch)
    {
    if(*ch == '\\' && *(ch+1) != ';')
      {
      ++ch;
      switch (*ch)
        {
        case '\\': result.insert(result.end(), '\\'); break;
        case '"': result.insert(result.end(), '"'); break;
        case ' ': result.insert(result.end(), ' '); break;
        case 't': result.insert(result.end(), '\t'); break;
        case 'n': result.insert(result.end(), '\n'); break;
        case 'r': result.insert(result.end(), '\r'); break;
        case '0': result.insert(result.end(), '\0'); break;
        case '\0':
          {
          cmSystemTools::Error("Trailing backslash in argument:\n", s);
          return result;
          }
        default:
          {
          std::string chStr(1, *ch);
          cmSystemTools::Error("Invalid escape sequence \\", chStr.c_str(),
                               "\nin argument ", s);
          }
        }
      }
    else
      {
      result.insert(result.end(), *ch);
      }
    }
  return result;
}

void cmSystemTools::Error(const char* m1, const char* m2,
                          const char* m3, const char* m4)
{
  std::string message = "CMake Error: ";
  if(m1)
    {
    message += m1;
    }
  if(m2)
    {
    message += m2;
    }
  if(m3)
    {
    message += m3;
    }
  if(m4)
    {
    message += m4;
    }
  cmSystemTools::s_ErrorOccured = true;
  cmSystemTools::Message(message.c_str(),"Error");
}


void cmSystemTools::SetErrorCallback(ErrorCallback f, void* clientData)
{
  s_ErrorCallback = f;
  s_ErrorCallbackClientData = clientData;
}

void cmSystemTools::Message(const char* m1, const char *title)
{
  if(s_DisableMessages)
    {
    return;
    }
  if(s_ErrorCallback)
    {
    (*s_ErrorCallback)(m1, title, s_DisableMessages, s_ErrorCallbackClientData);
    return;
    }
  else
    {
    std::cerr << m1 << std::endl << std::flush;
    }
  
}


void cmSystemTools::ReportLastSystemError(const char* msg)
{
  std::string m = msg;
  m += ": System Error: ";
  m += Superclass::GetLastSystemError();
  cmSystemTools::Error(m.c_str());
}

 
bool cmSystemTools::IsOn(const char* val)
{
  if (!val)
    {
    return false;
    }
  std::basic_string<char> v = val;
  
  for(std::basic_string<char>::iterator c = v.begin();
      c != v.end(); c++)
    {
    *c = toupper(*c);
    }
  return (v == "ON" || v == "1" || v == "YES" || v == "TRUE" || v == "Y");
}

bool cmSystemTools::IsNOTFOUND(const char* val)
{
  cmsys::RegularExpression reg("-NOTFOUND$");
  if(reg.find(val))
    {
    return true;
    }
  return std::string("NOTFOUND") == val;
}


bool cmSystemTools::IsOff(const char* val)
{
  if (!val || strlen(val) == 0)
    {
    return true;
    }
  std::basic_string<char> v = val;
  
  for(std::basic_string<char>::iterator c = v.begin();
      c != v.end(); c++)
    {
    *c = toupper(*c);
    }
  return (v == "OFF" || v == "0" || v == "NO" || v == "FALSE" || 
          v == "N" || cmSystemTools::IsNOTFOUND(v.c_str()) || v == "IGNORE");
}


bool cmSystemTools::RunCommand(const char* command, 
                               std::string& output,
                               const char* dir,
                               bool verbose,
                               int timeout)
{
  int dummy;
  return cmSystemTools::RunCommand(command, output, dummy, 
                                   dir, verbose, timeout);
}

bool cmSystemTools::RunCommand(const char* command, 
                               std::string& output,
                               int &retVal, 
                               const char* dir,
                               bool verbose,
                               int)
{
  if(s_DisableRunCommandOutput)
    {
    verbose = false;
    }
  
  std::vector<std::string> args;
  std::string arg;
  
  // Split the command into an argv array.
  for(const char* c = command; *c;)
    {
    // Skip over whitespace.
    while(*c == ' ' || *c == '\t')
      {
      ++c;
      }
    arg = "";
    if(*c == '"')
      {
      // Parse a quoted argument.
      ++c;
      while(*c && *c != '"')
        {
        if(*c == '\\')
          {
          ++c;
          if(*c)
            {
            arg.append(1, *c);
            ++c;
            }
          }
        else
          {
          arg.append(1, *c);
          ++c;
          }
        }
      if(*c)
        {
        ++c;
        }
      args.push_back(arg);
      }
    else if(*c)
      {
      // Parse an unquoted argument.
      while(*c && *c != ' ' && *c != '\t')
        {
        arg.append(1, *c);
        ++c;
        }
      args.push_back(arg);
      }
    }
  
  std::vector<const char*> argv;
  for(std::vector<std::string>::const_iterator a = args.begin();
      a != args.end(); ++a)
    {
    argv.push_back(a->c_str());
    }
  argv.push_back(0);
  
  if(argv.size() < 2)
    {
    return false;
    }
  
  output = "";
  cmsysProcess* cp = cmsysProcess_New();
  cmsysProcess_SetCommand(cp, &*argv.begin());
  cmsysProcess_SetWorkingDirectory(cp, dir);
  cmsysProcess_Execute(cp);
  
  char* data;
  int length;
  while(cmsysProcess_WaitForData(cp, (cmsysProcess_Pipe_STDOUT |
                                      cmsysProcess_Pipe_STDERR),
                                 &data, &length, 0))
    {
    output.append(data, length);
    if(verbose)
      {
      std::cout.write(data, length);
      }
    }
  
  cmsysProcess_WaitForExit(cp, 0);
  
  bool result = true;
  if(cmsysProcess_GetState(cp) == cmsysProcess_State_Exited)
    {
    retVal = cmsysProcess_GetExitValue(cp);
    }
  else
    {
    result = false;
    }
  
  cmsysProcess_Delete(cp);
  
  return result;
}

bool cmSystemTools::DoesFileExistWithExtensions(
  const char* name,
  const std::vector<std::string>& headerExts)
{
  std::string hname;

  for( std::vector<std::string>::const_iterator ext = headerExts.begin();
       ext != headerExts.end(); ++ext )
    {
    hname = name;
    hname += ".";
    hname += *ext;
    if(cmSystemTools::FileExists(hname.c_str()))
      {
      return true;
      }
    }
  return false;
}

bool cmSystemTools::cmCopyFile(const char* source, const char* destination)
{
  return Superclass::CopyFileAlways(source, destination);
}

void cmSystemTools::Glob(const char *directory, const char *regexp,
                         std::vector<std::string>& files)
{
  cmsys::Directory d;
  cmsys::RegularExpression reg(regexp);
  
  if (d.Load(directory))
    {
    size_t numf;
        unsigned int i;
    numf = d.GetNumberOfFiles();
    for (i = 0; i < numf; i++)
      {
      std::string fname = d.GetFile(i);
      if (reg.find(fname))
        {
        files.push_back(fname);
        }
      }
    }
}


void cmSystemTools::GlobDirs(const char *fullPath,
                             std::vector<std::string>& files)
{
  std::string path = fullPath;
  std::string::size_type pos = path.find("/*");
  if(pos == std::string::npos)
    {
    files.push_back(fullPath);
    return;
    }
  std::string startPath = path.substr(0, pos);
  std::string finishPath = path.substr(pos+2);

  cmsys::Directory d;
  if (d.Load(startPath.c_str()))
    {
    for (unsigned int i = 0; i < d.GetNumberOfFiles(); ++i)
      {
      if((std::string(d.GetFile(i)) != ".")
         && (std::string(d.GetFile(i)) != ".."))
        {
        std::string fname = startPath;
        fname +="/";
        fname += d.GetFile(i);
        if(cmSystemTools::FileIsDirectory(fname.c_str()))
          {
          fname += finishPath;
          cmSystemTools::GlobDirs(fname.c_str(), files);
          }
        }
      }
    }
}


void cmSystemTools::ExpandList(std::vector<std::string> const& arguments, 
                               std::vector<std::string>& newargs)
{
  std::vector<std::string>::const_iterator i;
  for(i = arguments.begin();i != arguments.end(); ++i)
    {
    cmSystemTools::ExpandListArgument(*i, newargs);
    }
}

void cmSystemTools::ExpandListArgument(const std::string& arg,
                                       std::vector<std::string>& newargs)
{
  std::string newarg;
  // If argument is empty, it is an empty list.
  if(arg.length() == 0)
    {
    return;
    }
  // if there are no ; in the name then just copy the current string
  if(arg.find(';') == std::string::npos)
    {
    newargs.push_back(arg);
    }
  else
    {
    std::string::size_type start = 0;
    std::string::size_type endpos = 0;
    const std::string::size_type size = arg.size();
    // break up ; separated sections of the string into separate strings
    while(endpos != size)
      {
      endpos = arg.find(';', start); 
      if(endpos == std::string::npos)
        {
        endpos = arg.size();
        }
      else
        {
        // skip right over escaped ; ( \; )
        while((endpos != std::string::npos)
              && (endpos > 0) 
              && ((arg)[endpos-1] == '\\') )
          {
          endpos = arg.find(';', endpos+1);
          }
        if(endpos == std::string::npos)
          {
          endpos = arg.size();
          }
        }
      std::string::size_type len = endpos - start;
      if (len > 0)
        {
        // check for a closing ] after the start position
        if(arg.find('[', start) == std::string::npos)
          {
          // if there is no [ in the string then keep it
          newarg = arg.substr(start, len);
          }
        else
          {
          int opencount = 0;
          int closecount = 0;
          for(std::string::size_type j = start; j < endpos; ++j)
            {
            if(arg.at(j) == '[')
              {
              ++opencount;
              }
            else if (arg.at(j) == ']')
              {
              ++closecount;
              }
            }
          if(opencount != closecount)
            {
            // skip this one
            endpos = arg.find(';', endpos+1);  
            if(endpos == std::string::npos)
              {
              endpos = arg.size();
              }
            len = endpos - start;
            }
          newarg = arg.substr(start, len);
          }
        std::string::size_type pos = newarg.find("\\;");
        if(pos != std::string::npos)
          {
          newarg.erase(pos, 1);
          }
        newargs.push_back(newarg);
        }
      start = endpos+1;
      }
    }
}

bool cmSystemTools::SimpleGlob(const std::string& glob, 
                               std::vector<std::string>& files, 
                               int type /* = 0 */)
{
  files.clear();
  if ( glob[glob.size()-1] != '*' )
    {
    return false;
    }
  std::string path = cmSystemTools::GetFilenamePath(glob);
  std::string ppath = cmSystemTools::GetFilenameName(glob);
  ppath = ppath.substr(0, ppath.size()-1);
  if ( path.size() == 0 )
    {
    path = "/";
    }

  bool res = false;
  cmsys::Directory d;
  if (d.Load(path.c_str()))
    {
    for (unsigned int i = 0; i < d.GetNumberOfFiles(); ++i)
      {
      if((std::string(d.GetFile(i)) != ".")
         && (std::string(d.GetFile(i)) != ".."))
        {
        std::string fname = path;
        if ( path[path.size()-1] != '/' )
          {
          fname +="/";
          }
        fname += d.GetFile(i);
        std::string sfname = d.GetFile(i);
        if ( type > 0 && cmSystemTools::FileIsDirectory(fname.c_str()) )
          {
          continue;
          }
        if ( type < 0 && !cmSystemTools::FileIsDirectory(fname.c_str()) )
          {
          continue;
          }
        if ( sfname.size() >= ppath.size() && 
             sfname.substr(0, ppath.size()) == 
             ppath )
          {
          files.push_back(fname);
          res = true;
          }
        }
      }
    }
  return res;
}

cmSystemTools::FileFormat cmSystemTools::GetFileFormat(const char* cext)
{
  if ( ! cext || *cext == 0 )
    {
    return cmSystemTools::NO_FILE_FORMAT;
    }
  //std::string ext = cmSystemTools::LowerCase(cext);
  std::string ext = cext;
  if ( ext == "c" || ext == ".c" ) { return cmSystemTools::C_FILE_FORMAT; }
  if ( 
    ext == "C" || ext == ".C" ||
    ext == "M" || ext == ".M" ||
    ext == "c++" || ext == ".c++" ||
    ext == "cc" || ext == ".cc" ||
    ext == "cpp" || ext == ".cpp" ||
    ext == "cxx" || ext == ".cxx" ||
    ext == "m" || ext == ".m" ||
    ext == "mm" || ext == ".mm"
    ) { return cmSystemTools::CXX_FILE_FORMAT; }
  if ( ext == "java" || ext == ".java" ) { return cmSystemTools::JAVA_FILE_FORMAT; }
  if ( 
    ext == "H" || ext == ".H" || 
    ext == "h" || ext == ".h" || 
    ext == "h++" || ext == ".h++" ||
    ext == "hm" || ext == ".hm" || 
    ext == "hpp" || ext == ".hpp" || 
    ext == "hxx" || ext == ".hxx" ||
    ext == "in" || ext == ".in" ||
    ext == "txx" || ext == ".txx"
    ) { return cmSystemTools::HEADER_FILE_FORMAT; }
  if ( ext == "rc" || ext == ".rc" ) { return cmSystemTools::RESOURCE_FILE_FORMAT; }
  if ( ext == "def" || ext == ".def" ) { return cmSystemTools::DEFINITION_FILE_FORMAT; }
  if ( ext == "lib" || ext == ".lib" ||
       ext == "a" || ext == ".a") { return cmSystemTools::STATIC_LIBRARY_FILE_FORMAT; }
  if ( ext == "o" || ext == ".o" ||
       ext == "obj" || ext == ".obj") { return cmSystemTools::OBJECT_FILE_FORMAT; }
#ifdef __APPLE__
  if ( ext == "dylib" || ext == ".dylib" ) 
    { return cmSystemTools::SHARED_LIBRARY_FILE_FORMAT; }
  if ( ext == "so" || ext == ".so" || 
       ext == "bundle" || ext == ".bundle" ) 
    { return cmSystemTools::MODULE_FILE_FORMAT; } 
#else // __APPLE__
  if ( ext == "so" || ext == ".so" || 
       ext == "sl" || ext == ".sl" || 
       ext == "dll" || ext == ".dll" ) 
    { return cmSystemTools::SHARED_LIBRARY_FILE_FORMAT; }
#endif // __APPLE__
  return cmSystemTools::UNKNOWN_FILE_FORMAT;
}

bool cmSystemTools::Split(const char* s, std::vector<cmStdString>& l)
{
  std::vector<std::string> temp;
  if(!Superclass::Split(s, temp))
    {
    return false;
    }
  for(std::vector<std::string>::const_iterator i = temp.begin();
      i != temp.end(); ++i)
    {
    l.push_back(*i);
    }
  return true;
}
