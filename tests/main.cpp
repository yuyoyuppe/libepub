#include <iostream>
#include <string>
#include <cstdarg>
#include <algorithm>
#include <limits>
#include <variant>
#include <random>

#include "libepub.h"

//#define CATCH_ERROR

int main()
{
  char const * paths[2] = {"../tests/feast.epub", "../tests/dance.epub"};

#ifdef CATCH_ERROR
  try
  {
#endif
    libepub::Book book1{paths[0]}, book2{paths[1]};
    libepub::Book combo{book1 + book2};
    shuffle(begin(combo._chapters), end(combo._chapters), std::mt19937{std::random_device{}()});
    combo.save("../tests/combo_output.epub");

    //for(auto pathToBook : paths)
    //{
    //  libepub::Book book{pathToBook};
    //  book.printMetadata(std::cout);
    //  for(auto && chapter : book._chapters)
    //    std::cout << "chapter = '" << chapter.name() << "'\n";
    //}

#ifdef CATCH_ERROR
  }
  catch(std::exception & e)
  {
    std::cout << "threw: " << e.what();
  }
#endif
  return 0;
}
