var laslibfilterslist = [
  "-keep_tile <ll_x> <ll_y> <size>",
  "-keep_circle <x> <y> <radius>",
  "-keep_xy <min_x> <min_y> <max_x> <max_y>",
  "-drop_xy <min_x> <min_y> <max_x> <max_y>",
  "-keep_x <min_x> <max_x>",
  "-drop_x <min_x> <max_x>",
  "-drop_x_below <min_x>",
  "-drop_x_above <max_x>",
  "-keep_y <min_y> <max_y>",
  "-drop_y <min_y> <max_y>",
  "-drop_y_below <min_y>",
  "-drop_y_above <max_y>",
  "-keep_z <min_z> <max_z>",
  "-drop_z <min_z> <max_z>",
  "-drop_z_below <min_z>",
  "-drop_z_above <max_z>",
  "-keep_xyz <min_x> <min_y> <min_z> <max_x> <max_y> <max_z>",
  "-drop_xyz <min_x> <min_y> <min_z> <max_x> <max_y> <max_z>",
  "-drop_duplicates",
  "-keep_first",
  "-first_only",
  "-drop_first",
  "-keep_last",
  "-last_only",
  "-drop_last",
  "-keep_second_last",
  "-drop_second_last",
  "-keep_first_of_many",
  "-keep_last_of_many",
  "-drop_first_of_many",
  "-drop_last_of_many",
  "-keep_middle",
  "-drop_middle",
  "-keep_return <v1> <v2> <v3>",
  "-drop_return <v1> <v2>",
  "-keep_single",
  "-drop_single",
  "-keep_double",
  "-drop_double",
  "-keep_triple",
  "-drop_triple",
  "-keep_quadruple",
  "-drop_quadruple",
  "-keep_number_of_returns <n>",
  "-drop_number_of_returns <n>",
  "-drop_scan_direction <v>",
  "-keep_scan_direction_change",
  "-keep_edge_of_flight_line",
  "-keep_intensity <min_int> <max_int>",
  "-drop_intensity_below <min_int>",
  "-drop_intensity_above <max_int>",
  "-drop_intensity_between <min_int> <max_int>",
  "-keep_class <c1> <c2> <c3>",
  "-drop_class <c1> <c2>",
  "-keep_extended_class <c>",
  "-drop_extended_class <c1> <c2>",
  "-drop_synthetic",
  "-keep_synthetic",
  "-drop_keypoint",
  "-keep_keypoint",
  "-drop_withheld",
  "-keep_withheld",
  "-drop_overlap",
  "-keep_overlap",
  "-keep_user_data <v>",
  "-drop_user_data <v>",
  "-keep_user_data_below <v>",
  "-keep_user_data_above <v>",
  "-keep_user_data_between <v1> <v2>",
  "-drop_user_data_below <v>",
  "-drop_user_data_above <v>",
  "-drop_user_data_between <v1> <v2>",
  "-keep_point_source <v>",
  "-keep_point_source_between <v1> <v2>",
  "-drop_point_source <v>",
  "-drop_point_source_below <v>",
  "-drop_point_source_above <v>",
  "-drop_point_source_between <v1> <v2>",
  "-keep_scan_angle <min_angle> <max_angle>",
  "-drop_abs_scan_angle_above <max_abs_angle>",
  "-drop_abs_scan_angle_below <min_abs_angle>",
  "-drop_scan_angle_below <min_angle>",
  "-drop_scan_angle_above <max_angle>",
  "-drop_scan_angle_between <min_angle> <max_angle>",
  "-keep_gps_time <min_time> <max_time>",
  "-drop_gps_time_below <min_time>",
  "-drop_gps_time_above <max_time>",
  "-drop_gps_time_between <min_time> <max_time>",
  "-keep_RGB_red <min_red> <max_red>",
  "-drop_RGB_red <min_red> <max_red>",
  "-keep_RGB_green <min_green> <max_green>",
  "-drop_RGB_green <min_green> <max_green>",
  "-keep_RGB_blue <min_blue> <max_blue>",
  "-keep_RGB_nir <min_nir> <max_nir>",
  "-keep_RGB_greenness <min_greenness> <max_greenness>",
  "-keep_NDVI <min_NDVI> <max_NDVI>",
  "-keep_NDVI_from_CIR <min_NDVI> <max_NDVI>",
  "-keep_NDVI_intensity_is_NIR <min_NDVI> <max_NDVI>",
  "-keep_NDVI_green_is_NIR <min_NDVI> <max_NDVI>",
  "-keep_wavepacket <v>",
  "-drop_wavepacket <v>",
  "-keep_attribute_above <attr> <min_value>",
  "-drop_attribute_below <attr> <min_value>",
  "-keep_every_nth <n>",
  "-drop_every_nth <n>",
  "-keep_random_fraction <fraction>",
  "-keep_random_fraction <fraction> <seed>",
  "-thin_with_grid <grid_size>",
  "-thin_pulses_with_time <time>",
  "-thin_points_with_time <time>",
  "-filter_and"
];

var attributes = [
  'x', 'y', 'z', 't', 'a', 'i', 'n', 'r', 'c', 's', 'k', 'w', 'o', 'u', 'p', 'e', 'd', 'R', 'G', 'B', 'N',
  'gpstime', 'angle', 'intensity', 'numberofreturns', 'returnnumber', 'classification', 'synthetic', 'keypoint', 'withheld', 'overlap', 'userdata', 'pointsourceid', 'edgeofflightline', 'scandirectionflag', 'red', 'green', 'blue', 'nir'
];

var metrics = ['mean', 'sd', 'cv', 'min', 'max', 'sum', 'mode', 'count', 'median', 'pX', 'aboveX'];

const attributeMetricCombinations = [];

attributes.forEach(attribute => {
  metrics.forEach(metric => {
    attributeMetricCombinations.push(`${attribute}_${metric}`);
  });
});

function initializeAwesomplete(selector, list) {
  document.querySelectorAll(selector).forEach(function (input) {
    console.log("Initializing Awesomplete on:", input);
    new Awesomplete(input, {
      list: list,
      filter: function (text, input) {
        var lastValue = input.split(/,\s*/).pop();
        return Awesomplete.FILTER_CONTAINS(text, lastValue);
      },
      item: function (text, input) {
        var lastValue = input.split(/,\s*/).pop();
        return Awesomplete.ITEM(text, lastValue);
      },
      replace: function (text) {
        var parts = this.input.value.split(/,\s*/);
        parts.pop();
        parts.push(text);
        this.input.value = parts.join(", ");
      }
    });
  });
}