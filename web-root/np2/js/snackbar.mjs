
import Constants from './constants.mjs';

/*
 * Simple SnackBar messages
 * (from https://www.w3schools.com/howto/howto_js_snackbar.asp)
 */
function SnackBarMessage(message) {
    let x = document.getElementById(Constants.ELEMENT_SNACKBAR);
    if( x ) {
        // show snackbar message only when the snackbar element id exist
        // changing snackBar 
        x.className = "show";
        x.innerText = message;
        setTimeout(function(){ 
            x.className = x.className.replace("show", ""); 
        }, Constants.SNACKBAR_TIMEOUT);
    }
}

export default SnackBarMessage;

