function generateUUID() {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
    var r = Math.random() * 16 | 0, v = c == 'x' ? r : (r & 0x3 | 0x8);
    return v.toString(16);
  });
}

document.addEventListener('keydown', function (event) {
  if (event.ctrlKey && event.key === ' ') {
    event.preventDefault();
    var json = editor.export();
    json.drawflow.uuid = generateUUID();
    const cleanedData = removeHtmlProperties(json);
    Swal.fire({
      title: 'JSON',
      html: '<pre class="json-content">' + syntaxHighlight(JSON.stringify(cleanedData, null, 2)) + '</pre>'
    });
  }
});

function escapeHtml(unsafe) {
  return unsafe.replace(/[&<"']/g, function (m) {
    switch (m) {
      case '&':
        return '&amp;';
      case '<':
        return '&lt;';
      case '"':
        return '&quot;';
      case "'":
        return '&#39;';
      default:
        return m;
    }
  });
}

function removeHtmlProperties(obj) {
  if (Array.isArray(obj)) {
    return obj.map(removeHtmlProperties); // Recursively process each item in the array
  } else if (typeof obj === 'object' && obj !== null) {
    return Object.keys(obj).reduce((result, key) => {
      if (key !== 'html') { // Exclude the 'html' key
        result[key] = removeHtmlProperties(obj[key]); // Recursively process each property
      }
      return result;
    }, {});
  }
  return obj; // Return non-object values unchanged
}

function syntaxHighlight(json) {
  json = json.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  return json.replace(/("(\\u[a-zA-Z0-9]{4}|\\[^u]|[^\\"])*"(\s*:)?|\b(true|false|null)\b|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?)/g, function (match) {
    var cls = 'number';
    if (/^"/.test(match)) {
      if (/:$/.test(match)) {
        cls = 'key';
      } else {
        cls = 'string';
      }
    } else if (/true|false/.test(match)) {
      cls = 'boolean';
    } else if (/null/.test(match)) {
      cls = 'null';
    }
    return '<span class="' + cls + '">' + match + '</span>';
  });
}