#include <fstream>
#include <iostream>
#include <sstream>
#include <functional>

#include "utils.h"
#include "miniz_zip.h"

#include "libepub.h"

namespace libepub {

using namespace detail;

Resource::Resource(std::string const name, char const * const content, std::size_t const size)
  : _content(content, size), _path(name)
{
}

Book::Book(std::string const & path_to_epub)
{
  std::ifstream file{path_to_epub.c_str(), std::ios_base::binary | std::ios_base::in};
  if(!file.is_open())
    fatal("can't open %s", path_to_epub.c_str());

  std::string const buffer{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  initializeFromBuffer(buffer.c_str(), buffer.size());
}

Book::Book(char const * buf, std::size_t const size) { initializeFromBuffer(buf, size); }

Book::Book(Book const & rhs)
  : _spineIDs(rhs._spineIDs)
  , _metadata(rhs._metadata)
  , _chapters(rhs._chapters)
  , _rootFilename(rhs._rootFilename)
  , _resources(rhs._resources)
{
  sort(begin(_resources), end(_resources), Resource::Sort{});
  auto it = unique(begin(_resources), end(_resources), Resource::Compare{});
  _resources.erase(it, cend(_resources));
}
Book Book::operator+(Book const & rhs)
{
  Book result{*this};
  result._metadata.insert(rhs._metadata.begin(), rhs._metadata.end());
  result._chapters.insert(result._chapters.end(), rhs._chapters.begin(), rhs._chapters.end());
  result._resources.insert(result._resources.end(), rhs._resources.begin(), rhs._resources.end());
  return result;
}

void removeNodes(pugi::xpath_node_set const &                nodeSet,
                 std::function<bool(pugi::xml_node const &)> needRemoval = [](auto) { return true; })
{
  for(auto const & xnode : nodeSet)
  {
    pugi::xml_node const & node = xnode.node();
    if(needRemoval(node))
      node.parent().remove_child(node);
  }
}

void Book::updateRootFile() const
{
  auto const & xml = getXMLRepresentation(resource(_rootFilename));

  auto const isInSpine = [this](char const * attributeName, pugi::xml_node const & node) {
    //TODO: sort spineids and use binary_find
    return end(_spineIDs) != find_if(begin(_spineIDs), end(_spineIDs), [&](char const * spineID) {
             return 0 == ::strcmp(node.attribute(attributeName).as_string(), spineID);
           });
  };
  removeNodes(xml.select_nodes("/package/manifest/item"), [&](auto n) { return isInSpine("id", n); });
  removeNodes(xml.select_nodes("/package/spine/itemref"), [&](auto n) { return isInSpine("idref", n); });

  pugi::xml_node manifestNode = xml.select_node("/package/manifest").node();
  pugi::xml_node spineNode    = xml.select_node("/package/spine").node();

  for(int idx = 0; idx < _chapters.size(); ++idx)
  {
    Resource const & r = resource(_chapters[idx].associatedResourceName());

    pugi::xml_node newManifestNode = manifestNode.append_child("item");
    pugi::xml_node newSpineNode    = spineNode.append_child("itemref");

    int constexpr idOffset = 10000;
    auto const idxStr{std::to_string(idx + idOffset)};

    newManifestNode.append_attribute("href").set_value(r._path.c_str());
    newManifestNode.append_attribute("id").set_value(idxStr.c_str());
    newManifestNode.append_attribute("media-type").set_value(r._kind.c_str());

    newSpineNode.append_attribute("idref").set_value(idxStr.c_str());
    newSpineNode.append_attribute("linear").set_value("yes");
  }
}

void Book::save(std::string const & filename) const
{
  mz_zip_archive zip_archive{};
  if(!mz_zip_writer_init_file_v2(&zip_archive, filename.c_str(), 0, 0))
    fatal("mz_zip_writer_init_v2 failed!");

  updateRootFile();

  for(Resource const & r : _resources)
  {
    const bool         hasXML = hasXMLRepresentation(r);
    std::ostringstream iss;
    if(hasXML)
    {
      auto const & xml = getXMLRepresentation(r);
      xml.save(iss, "", pugi::format_raw);
      //xml.save(iss);
    }
    std::string const & source = hasXML ? iss.str() : r._content;

    if(!mz_zip_writer_add_mem(&zip_archive, r._path.c_str(), source.c_str(), source.size(), MZ_DEFAULT_LEVEL))
      fatal("mz_zip_writer_add_mem error!");
  }

  mz_zip_writer_finalize_archive(&zip_archive); // TODO: handle errors
  mz_zip_writer_end(&zip_archive);
}

void Book::printMetadata(std::ostream & output)
{
  for(auto const && [name, value] : _metadata)
    output << name << ": " << value << '\n';
}

Resource const & Book::resource(std::string const & path) const
{
  auto const it = binary_find(cbegin(_resources), cend(_resources), Resource{path, nullptr, 0}, Resource::Sort{});
  if(it == cend(_resources))
  {
    fatal("requested non-existent resource %s", path.c_str());
  }
  return *it;
}

pugi::xml_document & Book::getXMLRepresentation(Resource const & r) const
{
  if(r._kind.find("xml") == std::string::npos)
    fatal("trying to get XML representation from incompatible resource kind!");

  auto const it = _parsedXMLResources.find(r.ID());
  if(it == _parsedXMLResources.cend())
  {
    pugi::xml_parse_result const parseRes =
      _parsedXMLResources[r.ID()].load_buffer((void *)(r._content.c_str()), r._content.size());
    if(parseRes.status != pugi::status_ok)
    {
      fatal("Couldn't parse " + r._path + ": " + parseRes.description());
    }
    return _parsedXMLResources[r.ID()];
  }
  return it->second;
}

bool Book::hasXMLRepresentation(Resource const & r) const
{
  return _parsedXMLResources.find(r.ID()) != end(_parsedXMLResources);
}

void Book::loadMetadata(Resource const & rootfile)
{
  char const * const metadataSpec[][2] = {{"title", "/package/metadata/dc:title"},
                                          {"author", "/package/metadata/dc:creator"},
                                          {"language", "/package/metadata/dc:language"},
                                          {"uuid", "/package/metadata/dc:identifier[@opf:scheme='uuid']"},
                                          {"isbn", "/package/metadata/dc:identifier[@opf:scheme='ISBN']"},
                                          {"description", "/package/metadata/dc:description"}};

  auto const & xml = getXMLRepresentation(rootfile);

  for(auto const && [name, path] : metadataSpec)
    _metadata[name] = xml.select_node(path).node().text().as_string();
}

// TODO: move to detail. also need to have a customizable callback in public API ._.
struct text_walker : pugi::xml_tree_walker
{
  std::string _text;

  bool for_each(pugi::xml_node & node) override
  {
    _text.append(node.text().as_string());
    bool const continueTraversing = _text.empty();
    return continueTraversing;
  }
};

std::string Book::deduceChapterName(Resource const & chapter) const
{
  auto const & xml = getXMLRepresentation(chapter);
  for(char idx = '5'; idx > '0'; idx--)
  {
    char const   headerTag[] = {'/', '/', 'h', idx, '\0'};
    auto const & headers     = xml.select_nodes(headerTag);

    for(auto const & header : headers)
      if(header)
      {
        text_walker walker;
        header.node().traverse(walker);
        if(!walker._text.empty())
          return trim(walker._text);
      }
  }

  return "Untitled";
}

void Book::loadChapters(Resource const & rootfile)
{
  auto const & xml = getXMLRepresentation(rootfile);

  using manifest_item_t = std::tuple<std::string, std::string, std::string>; // id, href, media-type // TODO: ffs
  std::vector<manifest_item_t> manifest;

  auto const sortByID = tupleElementSorter<0>();
  {
    auto const manifestNodes = xml.select_nodes("/package/manifest/item");
    manifest.reserve(manifestNodes.size());
    for(auto const & n : manifestNodes)
      manifest.emplace_back(n.node().attribute("id").as_string(),
                            n.node().attribute("href").as_string(),
                            n.node().attribute("media-type").as_string());
    sort(begin(manifest), end(manifest), sortByID);

    auto const spineNodes = xml.select_nodes("/package/spine/itemref");
    _spineIDs.reserve(spineNodes.size());
    for(auto const & n : spineNodes)
      _spineIDs.emplace_back(n.node().attribute("idref").as_string());
  }
  for(char const * const idref : _spineIDs)
  {
    manifest_item_t chapterID{idref, "", ""};

    auto const it = binary_find(cbegin(manifest), cend(manifest), chapterID, sortByID);
    if(it == cend(manifest))
      fatal(std::string{"couldn't match spine itemref with manifest item "} + idref);
    auto const & res = resource(std::get<1>(*it));

    const_cast<Resource &>(res)._kind = std::get<2>(*it);
    _chapters.emplace_back(std::move(deduceChapterName(res)), res._path);
  }
}

void Book::addResource(Resource && r)
{
  auto const pos = lower_bound(begin(_resources), end(_resources), r, Resource::Sort{});
  _resources.insert(pos, std::move(r));
}

void Book::initializeFromBuffer(char const * buf, std::size_t const size)
{
  mz_zip_archive epub_contents{};

  if(!mz_zip_reader_init_mem(&epub_contents, buf, size, 0))
    fatal("failed to unzip epub contents");

  mz_uint const nFiles = mz_zip_reader_get_num_files(&epub_contents);

  for(mz_uint i = 0; i < nFiles; i++)
  {
    mz_zip_archive_file_stat file_stat;
    if(!mz_zip_reader_file_stat(&epub_contents, i, &file_stat))
      fatal("mz_zip_reader_file_stat() failed!\n");
    bool const         isDir = mz_zip_reader_is_file_a_directory(&epub_contents, i);
    char const * const name  = file_stat.m_filename;
    if(!isDir)
    {
      std::size_t fileSize;
      auto        fileBuf = static_cast<char const *>(mz_zip_reader_extract_to_heap(&epub_contents, i, &fileSize, 0));
      if(!fileBuf)
        fatal("couldn't extract %s'", name);
      addResource({name, fileBuf, fileSize});
      delete fileBuf;
    }
  }

  mz_zip_reader_end(&epub_contents);

  Resource & container = const_cast<Resource &>(resource("META-INF/container.xml"));
  container._kind      = "application/xhtml+xml";

  auto const & containerXml = getXMLRepresentation(container);
  _rootFilename = containerXml.select_node("/container/rootfiles/rootfile").node().attribute("full-path").as_string();
  Resource & rootfile = const_cast<Resource &>(resource(_rootFilename));
  rootfile._kind      = "application/xhtml+xml";

  loadMetadata(rootfile);
  loadChapters(rootfile);
}

Chapter::Chapter(std::string const & name, std::string const & resourceName) : _resourceName(resourceName), _name(name)
{
}

} // namespace libepub
