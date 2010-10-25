/* Xmp for Android
 * Copyright (C) 2010 Claudio Matsuoka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

package org.helllabs.android.xmp;

import java.io.File;
import java.io.FilenameFilter;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;

public class ModList extends PlaylistActivity {
	static final int SETTINGS_REQUEST = 45;
	String media_path;
	Xmp xmp = new Xmp();	/* used to get mod info */
	boolean isBadDir = false;
	ProgressDialog progressDialog;
	final Handler handler = new Handler();
	
	class ModFilter implements FilenameFilter {
	    public boolean accept(File dir, String name) {
	    	//Log.v(getString(R.string.app_name), "** " + dir + "/" + name);
	        return (xmp.testModule(dir + "/" + name) == 0);
	    }
	}
	
	@Override
	public void onCreate(Bundle icicle) {
		setContentView(R.layout.modlist);
		super.onCreate(icicle);	
		ChangeLog changeLog = new ChangeLog(this);		
		registerForContextMenu(getListView());
		changeLog.show();		
		updatePlaylist();
	}

	public void updatePlaylist() {
		modList.clear();
		
		media_path = prefs.getString(Settings.PREF_MEDIA_PATH, Settings.DEFAULT_MEDIA_PATH);
		//Log.v(getString(R.string.app_name), "path = " + media_path);
		setTitle(media_path);
	
		final File modDir = new File(media_path);
		
		if (!modDir.isDirectory()) {
			final Examples examples = new Examples(this);
			
			isBadDir = true;
			AlertDialog alertDialog = new AlertDialog.Builder(this).create();
			
			alertDialog.setTitle("Oops");
			alertDialog.setMessage(media_path + " not found. " +
					"Create this directory or change the module path.");
			alertDialog.setButton("Create", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					examples.install(media_path,
							prefs.getBoolean(Settings.PREF_EXAMPLES, true));
					updatePlaylist();
				}
			});
			alertDialog.setButton2("Back", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					finish();
				}
			});
			alertDialog.show();
			return;
		}
		
		isBadDir = false;
		progressDialog = ProgressDialog.show(this,      
				"Please wait", "Scanning module files...", true);
		
		new Thread() { 
			public void run() { 		
            	for (File file : modDir.listFiles(new ModFilter())) {
            		ModInfo m = xmp.getModInfo(media_path + "/" + file.getName());
            		PlaylistInfo pi = new PlaylistInfo(m.name,
            				m.chn + " chn " + m.type, m.filename);
            		modList.add(pi);
            	}
            	
                final PlaylistInfoAdapter playlist = new PlaylistInfoAdapter(ModList.this,
                			R.layout.song_item, R.id.info, modList);
                
                /* This one must run in the UI thread */
                handler.post(new Runnable() {
                	public void run() {
                		 setListAdapter(playlist);
                	 }
                });
            	
                progressDialog.dismiss();
			}
		}.start();
	}
	
	@Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    	//Log.v(getString(R.string.app_name), requestCode + ":" + resultCode);
    	switch (requestCode) {
    	case SETTINGS_REQUEST:
            if (isBadDir || resultCode == RESULT_OK)
            	updatePlaylist();
            break;
        }
    }

	
	// Playlist context menu
	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
		int i = 0;
		menu.setHeaderTitle("Add to playlist");
		menu.add(Menu.NONE, i, i, "New playlist...");
		for (String each : PlaylistUtils.listNoSuffix()) {
			i++;
			menu.add(Menu.NONE, i, i, "Add to " + each);
		}
	}
	
	@Override
	public boolean onContextItemSelected(MenuItem item) {
		AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo)item.getMenuInfo();
		int index = item.getItemId();
		
		if (index == 0) {
			(new PlaylistUtils()).newPlaylist(this);
			return true;
		}
		index--;
		PlaylistInfo pi = modList.get(info.position);
		String line = pi.filename + ":" + pi.comment + ":" + pi.name;
		PlaylistUtils.addToList(this, PlaylistUtils.listNoSuffix()[index], line);

		return true;
	}


	// Menu
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
	    MenuInflater inflater = getMenuInflater();
	    inflater.inflate(R.menu.options_menu, menu);
	    return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch(item.getItemId()) {
		case R.id.menu_prefs:		
			startActivityForResult(new Intent(this, Settings.class), SETTINGS_REQUEST);
			break;
		case R.id.menu_refresh:
			updatePlaylist();
			break;
		}
		return true;
	}
}