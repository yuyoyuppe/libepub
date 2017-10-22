#pragma once

#include <string>
#include <vector>
#include <memory>
#include <set>
#include <variant>
#include <optional>
#include <unordered_map>

#include "pugixml.hpp" // should be in .cpp

namespace libepub {

namespace detail {

} // namespace detail

// TODO: this should also be in detail, since we're exposing fields etc.
struct Resource
{
  std::string _path;
  std::string _content;
  std::string _kind;

  inline std::string const & ID() const { return _path; }

  Resource(std::string const name, char const * const content, std::size_t const size);
  Resource(Resource const &) = default;
  Resource(Resource &&)      = default;
  Resource & operator=(Resource &&) = default;

  struct Sort
  {
    bool operator()(Resource const & lhs, Resource const & rhs) const
    {
      return lexicographical_compare(cbegin(lhs._path), cend(lhs._path), cbegin(rhs._path), cend(rhs._path));
    }
  };

  struct Compare
  {
    bool operator()(Resource const & lhs, Resource const & rhs) const { return lhs._path == rhs._path; }
  };

private:
  friend struct Sort;
  friend struct Compare;
};

struct Chapter
{
  std::string const & name() const { return _name; }

  std::string const & associatedResourceName() const { return _resourceName; }
  Chapter(std::string const & name, std::string const & _resourceName);

private:
  std::string _name;
  std::string _resourceName;
};

struct Book
{
  std::unordered_map<std::string, std::string> _metadata;
  std::vector<Chapter>                         _chapters;

  void save(std::string const & filename) const; // TODO: wtf with this asymmetry?
  void printMetadata(std::ostream & output);

  Book(std::string const & path_to_epub);
  Book(char const * buf, std::size_t const size);
  Book(Book const &);

  Book operator+(Book const & book);

private:
  void                 initializeFromBuffer(char const * buf, std::size_t const size);
  Resource const &     resource(std::string const & name) const;
  void                 addResource(Resource && r);
  void                 loadMetadata(Resource const & content);
  void                 loadChapters(Resource const & content);
  std::string          deduceChapterName(Resource const & resource) const;
  pugi::xml_document & getXMLRepresentation(Resource const & r) const;
  bool                 hasXMLRepresentation(Resource const & r) const;
  void                 updateRootFile() const;

  // TODO: should incapsulated into _internal_data/_internal_representation
  //{
  std::vector<char const *> _spineIDs;
  std::string               _rootFilename = "";
  std::vector<Resource>     _resources;
  std::unordered_map<std::string, pugi::xml_document> mutable _parsedXMLResources;
  //}
};
} // namespace libepub
