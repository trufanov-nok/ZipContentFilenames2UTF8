# ZipContentFilenames2UTF8

This is a small crossplatform tool made to find zip archieves that contain filenames in encoding other than UTF-8.  
It respects optional UTF-8 filenames specified via Info-ZIP Unicode Path Extra Field.  
It ignores filenames in ASCII.  
It can automatically convert filenames in CP866 and Windows-1251 to UTF-8 and repack zip file.  
It's based on *libzip*.  
  
Made for one of public web libraries to fix zip archieves that were compressed years ago on Windows machine with CP866 as filename encoding. Because such zip files are incorrectly displayed on Linux and MAC machines.
