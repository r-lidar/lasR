#include "setcrs.h"

LASRsetcrs::LASRsetcrs(int epsg)
{
  crs = CRS(epsg, true);
}

LASRsetcrs::LASRsetcrs(const std::string& wkt)
{
  crs = CRS(wkt, true);
}

bool LASRsetcrs::process(LASheader*& header)
{
  if (header->get_global_encoding_bit(4))
  {
    std::string wkt = crs.get_wkt();
    header->del_geo_ogc_wkt();
    header->set_geo_ogc_wkt(wkt.size(), wkt.c_str(), false);
  }
  else
  {
    int nitem = 1;
    int epsg = crs.get_epsg();

    LASvlr_key_entry* vlr = new LASvlr_key_entry[nitem];
    vlr[0].key_id = 3072;
    vlr[0].tiff_tag_location = 0;
    vlr[0].count = 1;
    vlr[0].value_offset = epsg;

    header->remove_vlr("LASF_Projection", 34735);
    header->del_geo_double_params();
    header->del_geo_ascii_params();
    header->set_geo_keys(nitem, vlr);

    delete[] vlr;
  }

  return true;
}