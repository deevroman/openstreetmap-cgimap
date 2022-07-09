#include "cgimap/api07/map_handler.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/options.hpp"
#include <fmt/core.h>
#include <map>


using std::string;
using std::auto_ptr;
using std::map;

namespace api07 {

map_responder::map_responder(mime::type mt, bbox b, data_selection &x)
    : osm_current_responder(mt, x, std::optional<bbox>(b)) {
  // create temporary tables of nodes, ways and relations which
  // are in or used by elements in the bbox
  int num_nodes = sel.select_nodes_from_bbox(b, global_settings::get_map_max_nodes());

  if (num_nodes > global_settings::get_map_max_nodes()) {
    throw http::bad_request(
        fmt::format("You requested too many nodes (limit is {}). "
                "Either request a smaller area, or use planet.osm"),
            global_settings::get_map_max_nodes());
  }
  // Short-circuit empty areas
  if (num_nodes > 0) {
    sel.select_ways_from_nodes();
    sel.select_nodes_from_way_nodes();
    sel.select_relations_from_ways();
    sel.select_relations_from_nodes();
    sel.select_relations_from_relations();
  }
}

map_handler::map_handler(request &req) : bounds(validate_request(req)) {}

map_handler::map_handler(request &req, int) {}

string map_handler::log_name() const {
  return (fmt::format("map({},{},{},{})", bounds.minlon,
          bounds.minlat, bounds.maxlon, bounds.maxlat));
}

responder_ptr_t map_handler::responder(data_selection &x) const {
  return responder_ptr_t(new map_responder(mime_type, bounds, x));
}

namespace {
bool is_bbox(const std::pair<string, string> &p) {
  return p.first == "bbox";
}
} // anonymous namespace

/**
 * Validates an FCGI request, returning the valid bounding box or
 * throwing an error if there was no valid bounding box.
 */
bbox map_handler::validate_request(request &req) {
  string decoded = http::urldecode(get_query_string(req));
  const std::vector<std::pair<string, string> > params = http::parse_params(decoded);
  auto itr =
    std::find_if(params.begin(), params.end(), is_bbox);

  bbox bounds;
  if ((itr == params.end()) || !bounds.parse(itr->second)) {
    throw http::bad_request("The parameter bbox is required, and must be "
                            "of the form min_lon,min_lat,max_lon,max_lat.");
  }

  // clip the bounding box against the world
  bounds.clip_to_world();

  // check that the bounding box is within acceptable limits. these
  // limits taken straight from the ruby map implementation.
  if (!bounds.valid()) {
    throw http::bad_request("The latitudes must be between -90 and 90, "
                            "longitudes between -180 and 180 and the "
                            "minima must be less than the maxima.");
  }

  if (bounds.area() > global_settings::get_map_area_max()) {
    throw http::bad_request(
         fmt::format("The maximum bbox size is {}, and your request "
                       "was too large. Either request a smaller area, or use "
                       "planet.osm", global_settings::get_map_area_max()));
  }

  return bounds;
}

} // namespace api07
