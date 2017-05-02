/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "oc_collection.h"

#if defined(OC_COLLECTIONS) && defined(OC_SERVER)
#include "oc_api.h"
#include "oc_core_res.h"
#include "util/oc_memb.h"

OC_MEMB(oc_collections_s, oc_collection_t, OC_MAX_NUM_COLLECTIONS);
OC_LIST(oc_collections);
OC_MEMB(oc_links_s, oc_link_t, OC_MAX_APP_RESOURCES);

oc_collection_t *
oc_collection_alloc(void)
{
  oc_collection_t *collection = oc_memb_alloc(&oc_collections_s);
  if (collection) {
    OC_LIST_STRUCT_INIT(collection, links);
    return collection;
  }
  return NULL;
}

oc_link_t *
oc_new_link(oc_resource_t *resource)
{
  oc_link_t *link = oc_memb_alloc(&oc_links_s);
  if (link) {
    oc_new_string_array(&link->rel, 3);
    oc_string_array_add_item(link->rel, "hosts");
    link->resource = resource;
    link->next = 0;
    memset(&link->ins, 0, sizeof(oc_string_t));
  }
  return link;
}

void
oc_link_set_ins(oc_link_t *link, const char *ins)
{
  oc_new_string(&link->ins, ins, strlen(ins));
}

void
oc_collection_add_link(oc_resource_t *collection, oc_link_t *link)
{
  oc_collection_t *c = (oc_collection_t *)collection;
  oc_list_add(c->links, link);
  if (link->resource == collection) {
    oc_string_array_add_item(link->rel, "self");
  }
}

void
oc_link_add_rel(oc_link_t *link, const char *rel)
{
  oc_string_array_add_item(link->rel, rel);
}

oc_collection_t *
oc_get_collection_by_uri(const char *uri_path, int uri_path_len)
{
  while (uri_path[0] == '/') {
    uri_path++;
    uri_path_len--;
  }
  oc_collection_t *collection = oc_list_head(oc_collections);
  while (collection != NULL) {
    if ((int)oc_string_len(collection->uri) == (uri_path_len + 1) &&
        strncmp(oc_string(collection->uri) + 1, uri_path, uri_path_len) == 0)
      break;
    collection = collection->next;
  }
  return collection;
}

bool
oc_check_if_collection(oc_resource_t *resource)
{
  oc_collection_t *collection = oc_list_head(oc_collections);
  while (collection != NULL) {
    if ((oc_collection_t *)resource == collection)
      return true;
    collection = collection->next;
  }
  return false;
}

bool
oc_collection_add(oc_collection_t *collection)
{
  if (oc_list_length(collection->links) > 0) {
    oc_list_add(oc_collections, collection);
    return true;
  }
  return false;
}

static bool
oc_link_filter_rt(oc_link_t *link, const char *rt, int rt_len)
{
  bool match = true;
  if (rt_len > 0) {
    match = false;
    int i;
    for (i = 0;
         i < (int)oc_string_array_get_allocated_size(link->resource->types);
         i++) {
      int size = oc_string_array_get_item_size(link->resource->types, i);
      const char *t =
        (const char *)oc_string_array_get_item(link->resource->types, i);
      if (rt_len == size && strncmp(rt, t, rt_len) == 0) {
        match = true;
        break;
      }
    }
  }
  return match;
}

bool
oc_handle_collection_request(oc_method_t method, oc_request_t *request,
                             oc_interface_mask_t interface)
{
  int code = 69;
  char *rt;
  int rt_len =
    oc_ri_get_query_value(request->query, request->query_len, "rt", &rt);
  oc_collection_t *collection = (oc_collection_t *)request->resource;
  oc_link_t *link = oc_list_head(collection->links);
  switch (interface) {
  case OC_IF_BASELINE: {
    oc_rep_start_root_object();
    oc_process_baseline_interface(request->resource);
    oc_rep_set_array(root, links);
    while (link != NULL) {
      if (oc_link_filter_rt(link, rt, rt_len)) {
        oc_rep_object_array_start_item(links);
        oc_rep_set_text_string(links, href, oc_string(link->resource->uri));
        oc_rep_set_string_array(links, rt, link->resource->types);
        oc_core_encode_interfaces_mask(oc_rep_object(links),
                                       link->resource->interfaces);
        oc_rep_set_string_array(links, rel, link->rel);
        if (oc_string_len(link->ins) > 0) {
          oc_rep_set_text_string(links, ins, oc_string(link->ins));
        }
        oc_rep_set_object(links, p);
        oc_rep_set_uint(p, bm, (uint8_t)(link->resource->properties &
                                         ~(OC_PERIODIC | OC_SECURE)));
#ifdef OC_SECURITY
        if (link->resource->properties & OC_SECURE) {
          oc_rep_set_boolean(p, sec, true);
          oc_rep_set_uint(p, port, oc_connectivity_get_dtls_port());
        }
#endif /* OC_SECURITY */
        oc_rep_close_object(links, p);
        oc_rep_object_array_end_item(links);
      }
      link = link->next;
    }
    oc_rep_close_array(root, links);
    oc_rep_end_root_object();
  } break;
  case OC_IF_LL: {
    oc_rep_start_links_array();
    while (link != NULL) {
      if (oc_link_filter_rt(link, rt, rt_len)) {
        oc_rep_object_array_start_item(links);
        oc_rep_set_text_string(links, href, oc_string(link->resource->uri));
        oc_rep_set_string_array(links, rt, link->resource->types);
        oc_core_encode_interfaces_mask(oc_rep_object(links),
                                       link->resource->interfaces);
        oc_rep_set_string_array(links, rel, link->rel);
        if (oc_string_len(link->ins) > 0) {
          oc_rep_set_text_string(links, ins, oc_string(link->ins));
        }
        oc_rep_set_object(links, p);
        oc_rep_set_uint(p, bm, (uint8_t)(link->resource->properties &
                                         ~(OC_PERIODIC | OC_SECURE)));
#ifdef OC_SECURITY
        if (link->resource->properties & OC_SECURE) {
          oc_rep_set_boolean(p, sec, true);
          oc_rep_set_uint(p, port, oc_connectivity_get_dtls_port());
        }
#endif /* OC_SECURITY */
        oc_rep_close_object(links, p);
        oc_rep_object_array_end_item(links);
      }
      link = link->next;
    }
    oc_rep_end_links_array();
  } break;
  case OC_IF_B: {
    CborEncoder encoder, prev_link;
    oc_request_t rest_request = { 0 };
    oc_response_t response = { 0 };
    oc_response_buffer_t response_buffer;
    bool method_not_found = false, get_delete = false;
    oc_rep_t *rep = request->request_payload;

    response.response_buffer = &response_buffer;
    rest_request.response = &response;
    rest_request.origin = request->origin;

    oc_rep_start_links_array();
    memcpy(&encoder, &g_encoder, sizeof(CborEncoder));
    if (method == OC_GET || method == OC_DELETE) {
      get_delete = true;
    }

    if (get_delete) {
      rest_request.request_payload = rep;
      goto process_request;
    }

    while (rep != NULL) {
      switch (rep->type) {
      case OBJECT: {
        oc_rep_t *pay = rep->value.object;
        oc_string_t href;
        while (pay != NULL) {
          switch (pay->type) {
          case STRING:
            oc_new_string(&href, oc_string(pay->value.string),
                          oc_string_len(pay->value.string));
            break;
          case OBJECT:
            rest_request.request_payload = pay->value.object;
            break;
          default:
            break;
          }
          pay = pay->next;
        }
      process_request:
        link = oc_list_head(collection->links);
        while (link != NULL) {
          if (link->resource) {
            if (oc_link_filter_rt(link, rt, rt_len)) {
              if (!get_delete && oc_string_len(href) > 0 &&
                  memcmp(oc_string(href), oc_string(link->resource->uri),
                         oc_string_len(href)) != 0) {
                goto next;
              }

              memcpy(&prev_link, &links_array, sizeof(CborEncoder));
              oc_rep_object_array_start_item(links);

              rest_request.query = 0;
              rest_request.query_len = 0;

              oc_rep_set_text_string(links, href,
                                     oc_string(link->resource->uri));
              oc_rep_set_key(*oc_rep_object(links), "rep");
              memcpy(&g_encoder, &links_map, sizeof(CborEncoder));

              int size_before = oc_rep_finalize();
              rest_request.resource = link->resource;
              response_buffer.code = 0;
              response_buffer.response_length = 0;
              method_not_found = false;

              switch (method) {
              case OC_GET:
                if (link->resource->get_handler.cb)
                  link->resource->get_handler.cb(
                    &rest_request, link->resource->default_interface,
                    link->resource->get_handler.user_data);
                else
                  method_not_found = true;
                break;
              case OC_PUT:
                if (link->resource->put_handler.cb)
                  link->resource->put_handler.cb(
                    &rest_request, link->resource->default_interface,
                    link->resource->get_handler.user_data);
                else
                  method_not_found = true;
                break;
              case OC_POST:
                if (link->resource->post_handler.cb)
                  link->resource->post_handler.cb(
                    &rest_request, link->resource->default_interface,
                    link->resource->get_handler.user_data);
                else
                  method_not_found = true;
                break;
              case OC_DELETE:
                if (link->resource->delete_handler.cb)
                  link->resource->delete_handler.cb(
                    &rest_request, link->resource->default_interface,
                    link->resource->get_handler.user_data);
                else
                  method_not_found = true;
                break;
              }

              if (method_not_found ||
                  response_buffer.code >=
                    oc_status_code(OC_STATUS_BAD_REQUEST)) {
                code = response_buffer.code;
                memcpy(&links_array, &prev_link, sizeof(CborEncoder));
                goto next;
              } else {
                if (code < oc_status_code(OC_STATUS_BAD_REQUEST))
                  code = response_buffer.code;
                int size_after = oc_rep_finalize();
                if (size_before == size_after) {
                  oc_rep_start_root_object();
                  oc_rep_end_root_object();
                }
              }

              memcpy(&links_map, &g_encoder, sizeof(CborEncoder));
              oc_rep_object_array_end_item(links);
            }
          }
        next:
          link = link->next;
        }
        if (get_delete)
          goto processed_request;
        if (oc_string_len(href) > 0) {
          oc_free_string(&href);
        }
      } break;
      default:
        break;
      }
      rep = rep->next;
    }

  processed_request:
    memcpy(&g_encoder, &encoder, sizeof(CborEncoder));
    oc_rep_end_links_array();
  } break;
  default:
    break;
  }

  int size = oc_rep_finalize();
  size = (size <= 2) ? 0 : size;

  request->response->response_buffer->response_length = size;
  request->response->response_buffer->code = code;

  return true;
}

oc_collection_t *
oc_collection_get_all(void)
{
  return (oc_collection_t *)oc_list_head(oc_collections);
}

#endif /* OC_COLLECTIONS && OC_SERVER */
