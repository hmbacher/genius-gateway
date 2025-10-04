export function jsonDateReviver(key: string, value: any) {
    // plug this regex into regex101.com to understand how it works
    // matches 2019-06-20T12:29:43.288Z (with milliseconds) and 2019-06-20T12:29:43Z (without milliseconds)
    var dateFormat = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d{1,}|)Z$/;

    if (typeof value === 'string' && dateFormat.exec(value)) {
        return new Date(value);
    }

    return value;
}

export function downloadObjectAsJson(exportObj: any, exportName: string, appendDateTimeStr: boolean = true, customSerializer?: ((this: any, key: string, value: any) => any) | undefined) {

    let appendStr = '';
    if (appendDateTimeStr) {
        const now = new Date();
        appendStr = `-${String(now.getFullYear())}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}-${String(now.getHours()).padStart(2, '0')}-${String(now.getMinutes()).padStart(2, '0')}-${String(now.getSeconds()).padStart(2, '0')}`;
    }

    var dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(exportObj, customSerializer, 2));

    var downloadAnchorNode = document.createElement('a');
    downloadAnchorNode.setAttribute("href", dataStr);
    downloadAnchorNode.setAttribute("download", exportName + appendStr);
    document.body.appendChild(downloadAnchorNode); // required for Firefox
    downloadAnchorNode.click();
    downloadAnchorNode.remove();
}