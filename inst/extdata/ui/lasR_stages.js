const reader_las = `
<div>
    <div class="title-box">
      <i class="fas fa-stream"></i> reader_las
      <a href="https://r-lidar.github.io/lasR/reference/reader_las.html" target="_blank"  title="Help"><i class="fas fa-question-circle"></i></a>
    </div>
    <div class="box">
    <div class="connectors">
        <div class="oconnectors"><p><span>cloud </span><i class="fas fa-spinner"></i></p></div>
    </div>
    <hr/>
    <div class="parameters">
        <p>filter: <input class="laslibfilter" type="text" df-filter></p>
    </div>
    </div>
</div>
`;

const write_las = `
<div>
    <div class="title-box">
      <i class="fas fa-file-pen"></i> write_las
      <a href="https://r-lidar.github.io/lasR/reference/write_las.html" target="_blank"  title="Help"><i class="fas fa-question-circle"></i></a>
    </div>
    <div class="box">
    <div class="connectors">
        <div class="iconnectors"><i class="fas fa-spinner"></i><p><span> cloud</span></p></div>
    </div>
    <hr/>
    <div class="parameters">
        <p>ofile: <input type="text" df-output></p>
        <p>filter: <input class="laslibfilter" type="text" df-filter></p>
        <p>keep_buffer:
        <select df-keep_buffer>
            <option value="false">false</option>
            <option value="true">true</option>
        </select>
        </p>
    </div>
    </div>
</div>
`;

const triangulate = `
<div>
    <div class="title-box">
      <i class="fas fa-square-caret-up"></i> triangulate
      <a href="https://r-lidar.github.io/lasR/reference/triangulate.html" target="_blank"  title="Help"><i class="fas fa-question-circle"></i></a>
    </div>
    <div class="box">
    <div class="connectors">
        <div class="iconnectors">
          <p><i class="fas fa-spinner"></i><span> cloud</span></p>
        </div>
        <div class="oconnectors">
          <p><span>tin </span><i class="fas fa-recycle"></i></p>
        </div>
    </div>
    <hr/>
    <div class="parameters">
      <p>max_edge: <input type="text" df-max_edge></p>
      <p>filter: <input class="laslibfilter" type="text" df-filter></p>
      <p>ofile: <input type="text" df-output></p>
      <p>use_attribute:
       <select df-use_attribute>
            <option value="Z">Z</option>
            <option value="Intensity">Intensity</option>
        </select></p>
    </div>
    </div>
</div>
`;

const transform_with = `
<div>
    <div class="title-box">
      <i class="fas fa-exchange-alt"></i> transform_with
     <a href="https://r-lidar.github.io/lasR/reference/transform_with.html" target="_blank"  title="Help"><i class="fas fa-question-circle"></i></a>
    </div>
    <div class="box">
        <div class="connectors">
            <div class="iconnectors">
              <p><i class="fas fa-spinner"></i><span> cloud</span></p>
              <p>raster | tin</p>
            </div>
            <div class="oconnectors"><p><span>cloud </span><i class="fas fa-spinner"></i></p></div>
        </div>
        <hr/>
        <div class="parameters">
          <p>operator:
           <select df-operator>
                <option value="-">-</option>
                <option value="+">+</option>
            </select> </p>
          <p>store_in_attribute: <input type="text" df-store_in_attribute value="Z"></p>
        </div>
    </div>
 </div>
`;

const rasterize = `
<div>
    <div class="title-box">
      <i class="fas fa-border-all"></i> rasterize
      <a href="https://r-lidar.github.io/lasR/reference/rasterize.html" target="_blank"  title="Help"><i class="fas fa-question-circle"></i></a>
    </div>
    <div class="box">
    <div class="connectors">
        <div class="iconnectors">
            <p><i class="fas fa-spinner"></i><span> cloud</span></p>
        </div>
        <div class="oconnectors">
          <p><span>raster </span><i class="fas fa-layer-group"></i></p>
        </div>
    </div>
    <hr/>
    <div class="parameters">
      <p>res: <input type="text" df-res></p>
      <p>operators: <input class="metricengine" type="text" df-method></p>
      <p>filter: <input class="laslibfilter" type="text" df-filter></p>
      <p>ofile:  <input type="text" df-output></p>
      <p>default value: <input type="text" df-default_value>
    </div>
    </div>
</div>
`;

const rasterize_tin = `
<div>
    <div class="title-box">
      <i class="fas fa-border-all"></i> rasterize
      <a href="https://r-lidar.github.io/lasR/reference/rasterize.html" target="_blank"  title="Help"><i class="fas fa-question-circle"></i></a>
    </div>
    <div class="box">
    <div class="connectors">
        <div class="iconnectors">
            <p><i class="fas fa-recycle"></i><span> tin</span></p>
        </div>
        <div class="oconnectors">
          <p><span>raster </span><i class="fas fa-layer-group"></i></p>
        </div>
    </div>
    <hr/>
    <div class="parameters">
      <p>res: <input type="text" df-res></p>
      <p>filter: <input class="laslibfilter" type="text" df-filter></p>
      <p>ofile:  <input type="text" df-output></p>
    </div>
    </div>
</div>
`;

const processing_options = `
<div>
    <div class="title-box">
      <i class="fas fa-shapes"></i> Processing options
      <a href="https://r-lidar.github.io/lasR/reference/exec.html" target="_blank"  title="Help"><i class="fas fa-question-circle"></i></a>
    </div>
    <div class="box">
    <div class="connectors"></div>
    <hr/>
    <div class="parameters">
      <p>files: <input type="text" df-files></p>
      <p>buffer: <input type="text" df-buffer></p>
      <p>chunk: <input type="text" df-chunk></p>
      <p>ncores: <input type="text" df-ncores></p>
      <p>strategy:
         <select df-strategy>
            <option value="concurrent-points">concurrent-points</option>
            <option value="concurrent-files">concurrent-files</option>
            <option value="nested">nested</option>
            <option value="sequential">sequential</option>
         </select>
      </p>
    </div>
    </div>
</div>
`;

const help = `
<div>
  <div class="title-box"><i class="fas fa-circle-question"></i> Welcome!!</div>
  <div class="box">
    <p>Visual pipeline programming for <a href="https://github.com/r-lidar/lasR" target="_blank">lasR</a></p>

    <p>Drag and drop stages<br>
      Connect stages by name/icon<br>
      Chose the data<br>
      Run the pipeline
    </p>

    <p><b><u>Shortkeys:</u></b></p>
    <p>🎹
    <b>Delete</b> for remove selected<br>
    💠 Mouse Left Click == Move<br>
    ❌ Mouse Right == Delete Option<br>
    🔍 Ctrl + Wheel == Zoom
    </p>
  </div>
</div>
`;

const dataToImport = {"drawflow":{"Home":{"data":{"52d81405-7985-4e6b-841b-5718e6537142":{"id":"52d81405-7985-4e6b-841b-5718e6537142","name":"processing_options","data":{"files":"#system.file(\"extdata\", \"Topography.las\", package = \"lasR\")#","ncores":"4"},"class":"processing_options","html":"\n<div>\n    <div class=\"title-box\">\n      <i class=\"fas fa-shapes\"></i> Processing options\n      <a href=\"https://r-lidar.github.io/lasR/reference/exec.html\" target=\"_blank\"  title=\"Help\"><i class=\"fas fa-question-circle\"></i></a>\n    </div>\n    <div class=\"box\">\n    <div class=\"connectors\"></div>\n    <hr/>\n    <div class=\"parameters\">\n      <p>files: <input type=\"text\" df-files></p>\n      <p>buffer: <input type=\"text\" df-buffer></p>\n      <p>chunk: <input type=\"text\" df-chunk></p>\n      <p>ncores: <input type=\"text\" df-ncores></p>\n      <p>strategy:\n         <select df-strategy>\n            <option value=\"concurrent-points\">concurrent-points</option>\n            <option value=\"concurrent-files\">concurrent-files</option>\n            <option value=\"nested\">nested</option>\n            <option value=\"sequential\">sequential</option>\n         </select>\n      </p>\n    </div>\n    </div>\n</div>\n","typenode":false,"inputs":{},"outputs":{},"pos_x":59,"pos_y":7},"5e4d8e55-9fce-4bf3-89eb-6e5123b4b253":{"id":"5e4d8e55-9fce-4bf3-89eb-6e5123b4b253","name":"reader_las","data":{"filter":"-keep_first"},"class":"reader_las","html":"\n<div>\n    <div class=\"title-box\">\n      <i class=\"fas fa-stream\"></i> reader_las\n      <a href=\"https://r-lidar.github.io/lasR/reference/reader_las.html\" target=\"_blank\"  title=\"Help\"><i class=\"fas fa-question-circle\"></i></a>\n    </div>\n    <div class=\"box\">\n    <div class=\"connectors\">\n        <div class=\"oconnectors\"><p><span>cloud </span><i class=\"fas fa-spinner\"></i></p></div>\n    </div>\n    <hr/>\n    <div class=\"parameters\">\n        <p>filter: <input class=\"laslibfilter\" type=\"text\" df-filter></p>\n    </div>\n    </div>\n</div>\n","typenode":false,"inputs":{},"outputs":{"output_1":{"connections":[{"node":"bc6a3d31-ff70-499f-95a3-89eee02fd028","output":"input_1"},{"node":"73063946-1d1d-4d1a-b786-420d20943a1a","output":"input_1"}]}},"pos_x":198,"pos_y":377},"bc6a3d31-ff70-499f-95a3-89eee02fd028":{"id":"bc6a3d31-ff70-499f-95a3-89eee02fd028","name":"rasterize","data":{"output":"#temptif()#","res":"20","method":"z_mean"},"class":"rasterize","html":"\n<div>\n    <div class=\"title-box\">\n      <i class=\"fas fa-border-all\"></i> rasterize\n      <a href=\"https://r-lidar.github.io/lasR/reference/rasterize.html\" target=\"_blank\"  title=\"Help\"><i class=\"fas fa-question-circle\"></i></a>\n    </div>\n    <div class=\"box\">\n    <div class=\"connectors\">\n        <div class=\"iconnectors\">\n            <p><i class=\"fas fa-spinner\"></i><span> cloud</span></p>\n        </div>\n        <div class=\"oconnectors\">\n          <p><span>raster </span><i class=\"fas fa-layer-group\"></i></p>\n        </div>\n    </div>\n    <hr/>\n    <div class=\"parameters\">\n      <p>res: <input type=\"text\" df-res></p>\n      <p>operators: <input class=\"metricengine\" type=\"text\" df-method></p>\n      <p>filter: <input class=\"laslibfilter\" type=\"text\" df-filter></p>\n      <p>ofile:  <input type=\"text\" df-output></p>\n      <p>default value: <input type=\"text\" df-default_value>\n    </div>\n    </div>\n</div>\n","typenode":false,"inputs":{"input_1":{"connections":[{"node":"5e4d8e55-9fce-4bf3-89eb-6e5123b4b253","input":"output_1"}]}},"outputs":{"output_1":{"connections":[]}},"pos_x":787,"pos_y":17},"73063946-1d1d-4d1a-b786-420d20943a1a":{"id":"73063946-1d1d-4d1a-b786-420d20943a1a","name":"rasterize","data":{"res":1,"method":"z_max","output":"#temptif()#"},"class":"Canopy Height Model","html":"\n<div>\n    <div class=\"title-box\">\n      <i class=\"fas fa-border-all\"></i> rasterize\n      <a href=\"https://r-lidar.github.io/lasR/reference/rasterize.html\" target=\"_blank\"  title=\"Help\"><i class=\"fas fa-question-circle\"></i></a>\n    </div>\n    <div class=\"box\">\n    <div class=\"connectors\">\n        <div class=\"iconnectors\">\n            <p><i class=\"fas fa-spinner\"></i><span> cloud</span></p>\n        </div>\n        <div class=\"oconnectors\">\n          <p><span>raster </span><i class=\"fas fa-layer-group\"></i></p>\n        </div>\n    </div>\n    <hr/>\n    <div class=\"parameters\">\n      <p>res: <input type=\"text\" df-res></p>\n      <p>operators: <input class=\"metricengine\" type=\"text\" df-method></p>\n      <p>filter: <input class=\"laslibfilter\" type=\"text\" df-filter></p>\n      <p>ofile:  <input type=\"text\" df-output></p>\n      <p>default value: <input type=\"text\" df-default_value>\n    </div>\n    </div>\n</div>\n","typenode":false,"inputs":{"input_1":{"connections":[{"node":"5e4d8e55-9fce-4bf3-89eb-6e5123b4b253","input":"output_1"}]}},"outputs":{"output_1":{"connections":[]}},"pos_x":787,"pos_y":420}}}}}

default_pipeline = {"drawflow":{"Home":{"data":{"52d81405-7985-4e6b-841b-5718e6537142":{"id":"52d81405-7985-4e6b-841b-5718e6537142","name":"processing_options","data":{},"class":"processing_options","html":"\n<div>\n    <div class=\"title-box\">\n      <i class=\"fas fa-shapes\"></i> Processing options\n      <a href=\"https://r-lidar.github.io/lasR/reference/exec.html\" target=\"_blank\"  title=\"Help\"><i class=\"fas fa-question-circle\"></i></a>\n    </div>\n    <div class=\"box\">\n    <div class=\"connectors\"></div>\n    <hr/>\n    <div class=\"parameters\">\n      <p>files: <input type=\"text\" df-files></p>\n      <p>buffer: <input type=\"text\" df-buffer></p>\n      <p>chunk: <input type=\"text\" df-chunk></p>\n      <p>ncores: <input type=\"text\" df-ncores></p>\n      <p>strategy:\n         <select df-strategy>\n            <option value=\"concurrent-points\">concurrent-points</option>\n            <option value=\"concurrent-files\">concurrent-files</option>\n            <option value=\"nested\">nested</option>\n            <option value=\"sequential\">sequential</option>\n         </select>\n      </p>\n    </div>\n    </div>\n</div>\n","typenode":false,"inputs":{},"outputs":{},"pos_x":59,"pos_y":7},"5e4d8e55-9fce-4bf3-89eb-6e5123b4b253":{"id":"5e4d8e55-9fce-4bf3-89eb-6e5123b4b253","name":"reader_las","data":{},"class":"reader_las","html":"\n<div>\n    <div class=\"title-box\">\n      <i class=\"fas fa-stream\"></i> reader_las\n      <a href=\"https://r-lidar.github.io/lasR/reference/reader_las.html\" target=\"_blank\"  title=\"Help\"><i class=\"fas fa-question-circle\"></i></a>\n    </div>\n    <div class=\"box\">\n    <div class=\"connectors\">\n        <div class=\"oconnectors\"><p><span>cloud </span><i class=\"fas fa-spinner\"></i></p></div>\n    </div>\n    <hr/>\n    <div class=\"parameters\">\n        <p>filter: <input class=\"laslibfilter\" type=\"text\" df-filter></p>\n    </div>\n    </div>\n</div>\n","typenode":false,"inputs":{},"outputs":{"output_1":{"connections":[]}},"pos_x":63,"pos_y":358}}}}}
