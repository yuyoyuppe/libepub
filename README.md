# libepub
Modular API for ePub books, currently in MVP state.

### Why?
Let's say you want to merge 2 books and reorder [their chapters accordingly](http://afeastwithdragons.com/). You can either script against rootfile.opf directly or use some wysiwyg and reorder manually, however, these are boring options! 

### Dependencies
* C++17 compiler(VS2017+ for Windows)
* premake5

### Getting started 
 E.g. on Linux:
 ```
 $ premake5 gmake
 $ make config=linux_release
  ```
### TODOs
* don't overwrite current book's `<manifest>` resources not referenced in `<spine>`
* proper error handling
* move internal book representation into `::detail`
* `XSD`s? :\ 
