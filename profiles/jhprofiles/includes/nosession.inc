<?php
/**
 * @file
 * User session handling functions.
 *
 * The user-level session storage handlers:
 * - _drupal_session_open()
 * - _drupal_session_close()
 * - _drupal_session_read()
 * - _drupal_session_write()
 * - _drupal_session_destroy()
 * - _drupal_session_garbage_collection()
 * are assigned by session_set_save_handler() in bootstrap.inc and are called
 * automatically by PHP. These functions should not be called directly. Session
 * data should instead be accessed via the $_SESSION superglobal.
 */

/**
 * Session handler assigned by session_set_save_handler().
 *
 * This function is used to handle any initialization, such as file paths or
 * database connections, that is needed before accessing session data. Drupal
 * does not need to initialize anything in this function.
 *
 * This function should not be called directly.
 *
 * @return
 *   This function will always return TRUE.
 */
function _drupal_session_open() {
  //本项目基于令牌token的会话存储，不需要指定session存储路径或者数据库连接地址
  return TRUE;
}




/**
 * Session handler assigned by session_set_save_handler().
 *
 * This function is used to close the current session. Because Drupal stores
 * session data in the database immediately on write, this function does
 * not need to do anything.
 *
 * This function should not be called directly.
 *
 * @return
 *   This function will always return TRUE.
 */
function _drupal_session_close() {
  //不需要做持久化动作，令牌存储在客户端，客户端负责持久化
  return TRUE;
}


/**
 * Reads an entire session from the database (internal use only).
 *
 * Also initializes the $user object for the user associated with the session.
 * This function is registered with session_set_save_handler() to support
 * database-backed sessions. It is called on every page load when PHP sets
 * up the $_SESSION superglobal.
 *
 * This function is an internal function and must not be called directly.
 * Doing so may result in logging out the current user, corrupting session data
 * or other unexpected behavior. Session data must always be accessed via the
 * $_SESSION superglobal.
 *
 * @param $sid
 *   The session ID of the session to retrieve.
 *
 * @return
 *   The user's session, or an empty string if no session exists.
 */
function _drupal_session_read () {
  
  global $user;
  // Write and Close handlers are called after destructing objects
  // since PHP 5.0.5.
  // Thus destructors can use sessions but session handler can't use objects.
  // So we are moving session closure before destructing objects.
  //shutdown函数会在程序执行完成后立即执行，比析构函数执行的要早
  drupal_register_shutdown_function('session_write_close');
  if(isset($_COOKIE[session_name()]) && $_COOKIE[session_name()]!='null'){
      $jwtToken=$_COOKIE[session_name()];
  }
   
  if (empty($jwtToken) ) {
    $user = drupal_anonymous_user();
    return '';
  }
 

  // Otherwise, if the session is still active
  // 恢复令牌中的用户信息
  require_once DRUPAL_ROOT . '/includes/common.inc';
  $redirect_url_parts = explode('?', $jwtToken);
  $redirect_url_params = drupal_get_query_array($redirect_url_parts[1]);
  $tokens = array(
        'id_token' => $redirect_url_params['id_token'],
        'access_token' => $redirect_url_params['access_token'],
      );
  
  
   $user_data = get_token($tokens);  
   if($user_data!='')  
   {
       $user_array=array('uid'=>(int)$user_data['sub'],
                     'roles'=>$user_data['roles'],
                     'hostname' => ip_address(),
					 'signature'=>$user_data['signature'],
					 'name'=>$user_data['username']);

       $user=(object)$user_array;
   }

  // We found the client's session record and they are an authenticated,
  // active user.
  //用户通过了认证
  if ($user && $user->uid > 0) {
    // This is done to unserialize the data member of $user.
    //$user->data = unserialize($user->data);
  }
  //匿名用户处理
  elseif ($user) {
    // The user is anonymous or blocked. Only preserve two fields from the
    // {sessions} table.
    $account = drupal_anonymous_user();
    $account->session = $user->session;
    $account->timestamp = $user->timestamp;
    $user = $account;
  }
  //令牌过期
  else {
    // The session has expired.
    $user = drupal_anonymous_user();
    $user->session = '';
  }

  // Store the session that was read for comparison in _drupal_session_write().
  $last_read = &drupal_static('drupal_session_last_read');
  $last_read = array(
    'sid' => $sid,
    'value' => $user->session,
  );
  return $user->session;
 }



/**
 * Writes an entire session to the database (internal use only).
 *
 * This function is registered with session_set_save_handler() to support
 * database-backed sessions.
 *
 * This function is an internal function and must not be called directly.
 * Doing so may result in corrupted session data or other unexpected behavior.
 * Session data must always be accessed via the $_SESSION superglobal.
 *
 * @param $sid
 *   The session ID of the session to write to.
 * @param $value
 *   Session data to write as a serialized string.
 *
 * @return
 *   Always returns TRUE.
 */
function _drupal_session_write($sid, $value) {
  global $user;
  // The exception handler is not active at this point, so we need to do it
  // manually.
  try {
    if (!drupal_save_session()) {
      // We don't have anything to do if we are not allowed to save the session.
      return TRUE;
    }

    // Check whether $_SESSION has been changed in this request.
    $last_read = &drupal_static('drupal_session_last_read');
    $is_changed = !isset($last_read) || $last_read['sid'] != $sid || $last_read['value'] !== $value;
 
    // For performance reasons, do not update the sessions table, unless
    // $_SESSION has changed or more than 180 has passed since the last update.
    if ($is_changed || !isset($user->timestamp) || REQUEST_TIME - $user->timestamp > variable_get('session_write_interval', 180)) {
      // Either ssid or sid or both will be added from $key below.
      $fields = array(
        'uid' => $user->uid,
        'cache' => isset($user->cache) ? $user->cache : 0,
        'hostname' => ip_address(),
        'session' => $value,
        'timestamp' => REQUEST_TIME,
      );
     //修改令牌，更新session和时间，待开发
     $_COOKIE['jhvege2'] =  $value;
     require_once DRUPAL_ROOT . '/sites/all/modules/contrib/oauth2_client/oauth2_client.inc;
     $_COOKIE[session_name()] = OAuth2\Client::loadToken('jhvege_client');
     //$_SESSION['oauth2_client']['token'][$key]),加载的东西只是session的一部分，其他应用保存的数据可能会丢失
    
    }

    // Likewise, do not update access time more than once per 180 seconds.
    if ($user->uid && REQUEST_TIME - $user->access > variable_get('session_write_interval', 180)) {
      db_update('users')
        ->fields(array(
          'access' => REQUEST_TIME
        ))
        ->condition('uid', $user->uid)
        ->execute();
    }

    return TRUE;
  }
  catch (Exception $exception) {
    require_once DRUPAL_ROOT . '/includes/errors.inc';
    // If we are displaying errors, then do so with no possibility of a further
    // uncaught exception being thrown.
    if (error_displayable()) {
      print '<h1>Uncaught exception thrown in session handler.</h1>';
      print '<p>' . _drupal_render_exception_safe($exception) . '</p><hr />';
    }
    return FALSE;
  }
}


/**
 * Initializes the session handler, starting a session if needed.
 */
function drupal_session_initialize() {
  global $user, $is_https;

  session_set_save_handler('_drupal_session_open', '_drupal_session_close', '_drupal_session_read', '_drupal_session_write', '_drupal_session_destroy', '_drupal_session_garbage_collection');

  // We use !empty() in the following check to ensure that blank session IDs
  // are not valid.
  if (!empty($_COOKIE[session_name()]) || ($is_https && variable_get('https', FALSE) && !empty($_COOKIE[substr(session_name(), 1)]))) {
    // If a session cookie exists, initialize the session. Otherwise the
    // session is only started on demand in drupal_session_commit(), making
    // anonymous users not use a session cookie unless something is stored in
    // $_SESSION. This allows HTTP proxies to cache anonymous pageviews.
    drupal_session_start();
    //认证用户root或者会话为非空时页面不可缓存
    if ($user->uid == 1 || !empty($_SESSION)) {
      drupal_page_is_cacheable(FALSE);
    }
  }
  else {
    // Set a session identifier for this request. This is necessary because
    // we lazily start sessions at the end of this request, and some
    // processes (like drupal_get_token()) needs to know the future
    // session ID in advance.
    $GLOBALS['lazy_session'] = TRUE;
    $user = drupal_anonymous_user();
    // Less random sessions (which are much faster to generate) are used for
    // anonymous users than are generated in drupal_session_regenerate() when
    // a user becomes authenticated.
    session_id(drupal_random_key());
    if ($is_https && variable_get('https', FALSE)) {
      $insecure_session_name = substr(session_name(), 1);
      $session_id = drupal_random_key();
      $_COOKIE[$insecure_session_name] = $session_id;
    }
  }
  date_default_timezone_set(drupal_get_user_timezone());
}






/**
 * Starts a session forcefully, preserving already set session data.
 *
 * @ingroup php_wrappers
 */
function drupal_session_start() {
  // Command line clients do not support cookies nor sessions.
  if (!drupal_session_started() && !drupal_is_cli()) {
    // Save current session data before starting it, as PHP will destroy it.
    $session_data = isset($_SESSION) ? $_SESSION : NULL;

    // Apply any overrides to the session cookie params.
    $params = $original_params = session_get_cookie_params();
    // PHP settings for samesite will be handled by _drupal_cookie_params().
    unset($params['samesite']);
    $params = _drupal_cookie_params($params);
    if ($params !== $original_params) {
      if (\PHP_VERSION_ID >= 70300) {
        session_set_cookie_params($params);
      }
      else {
        session_set_cookie_params($params['lifetime'], $params['path'], $params['domain'], $params['secure'], $params['httponly']);
      }
    }

    session_start();
    drupal_session_started(TRUE);

    // Restore session data.
    if (!empty($session_data)) {
      $_SESSION += $session_data;
    }
  }
}

/**
 * Commits the current session, if necessary.
 *
 * If an anonymous user already have an empty session, destroy it.
 */
function drupal_session_commit() {
  global $user, $is_https;

  if (!drupal_save_session()) {
    // We don't have anything to do if we are not allowed to save the session.
    return;
  }

  if (empty($user->uid) && empty($_SESSION)) {
    // There is no session data to store, destroy the session if it was
    // previously started.
    if (drupal_session_started()) {
      session_destroy();
    }
  }
  else {
    // There is session data to store. Start the session if it is not already
    // started.
    if (!drupal_session_started()) {
      drupal_session_start();
      if ($is_https && variable_get('https', FALSE)) {
        $insecure_session_name = substr(session_name(), 1);
        $params = session_get_cookie_params();
        $expire = $params['lifetime'] ? REQUEST_TIME + $params['lifetime'] : 0;
        $options = array(
          'expires' => $expire,
          'path' => $params['path'],
          'domain' => $params['domain'],
          'secure' => FALSE,
          'httponly' => $params['httponly'],
        );
        drupal_setcookie($insecure_session_name, $_COOKIE[$insecure_session_name], $options);
      }
    }
    // Write the session data.
    session_write_close();
  }
}

/**
 * Returns whether a session has been started.
 */
function drupal_session_started($set = NULL) {
  static $session_started = FALSE;
  if (isset($set)) {
    $session_started = $set;
  }
  return $session_started && session_id();
}

/**
 * Called when an anonymous user becomes authenticated or vice-versa.
 *
 * @ingroup php_wrappers
 */
function drupal_session_regenerate() {
  global $user, $is_https;
  // Nothing to do if we are not allowed to change the session.
  if (!drupal_save_session()) {
    return;
  }

  if ($is_https && variable_get('https', FALSE)) {
    $insecure_session_name = substr(session_name(), 1);
    if (!isset($GLOBALS['lazy_session']) && isset($_COOKIE[$insecure_session_name])) {
      $old_insecure_session_id = $_COOKIE[$insecure_session_name];
    }
    $params = session_get_cookie_params();
    $session_id = drupal_random_key();
    // If a session cookie lifetime is set, the session will expire
    // $params['lifetime'] seconds from the current request. If it is not set,
    // it will expire when the browser is closed.
    $expire = $params['lifetime'] ? REQUEST_TIME + $params['lifetime'] : 0;
    $options = array(
      'expires' => $expire,
      'path' => $params['path'],
      'domain' => $params['domain'],
      'secure' => FALSE,
      'httponly' => $params['httponly'],
    );
    drupal_setcookie($insecure_session_name, $session_id, $options);
    $_COOKIE[$insecure_session_name] = $session_id;
  }

  if (drupal_session_started()) {
    $old_session_id = session_id();
    _drupal_session_regenerate_existing();
  }
  else {
    session_id(drupal_random_key());
  }

  if (isset($old_session_id)) {
    $params = session_get_cookie_params();
    $expire = $params['lifetime'] ? REQUEST_TIME + $params['lifetime'] : 0;
    $options = array(
      'expires' => $expire,
      'path' => $params['path'],
      'domain' => $params['domain'],
      'secure' => $params['secure'],
      'httponly' => $params['httponly'],
    );
    //会话ID就是TOKEN，调用create_token,生产新的令牌 
    require_once DRUPAL_ROOT . '/sites/all/modules/contrib/oauth2_client/oauth2_client.inc;
    drupal_setcookie(session_name(), OAuth2\Client::loadToken('jhvege_client') , $options);//待实现
  }
  elseif (isset($old_insecure_session_id)) {
    //会话ID就是TOKEN，调用create_token,生产新的令牌 
    require_once DRUPAL_ROOT . '/sites/all/modules/contrib/oauth2_client/oauth2_client.inc;
    drupal_setcookie(session_name(), OAuth2\Client::loadToken('jhvege_client'), $options);//待实现
  }
  else {
    // Start the session when it doesn't exist yet.
    // Preserve the logged in user, as it will be reset to anonymous
    // by _drupal_session_read.
    $account = $user;
    drupal_session_start();
    $user = $account;
  }
  date_default_timezone_set(drupal_get_user_timezone());
}

/**
 * Regenerates an existing session.
 */
function _drupal_session_regenerate_existing() {
  global $user;
  // Preserve existing settings for the saving of sessions.
  $original_save_session_status = drupal_save_session();
  // Turn off saving of sessions.
  drupal_save_session(FALSE);
  session_write_close();
  drupal_session_started(FALSE);
  // Preserve the user object, as starting a new session will reset it.
  $original_user = $user;
  session_id(drupal_random_key());//只是生成临时
  drupal_session_start();
  $user = $original_user;
  // Restore the original settings for the saving of sessions.
  drupal_save_session($original_save_session_status);
}

/**
 * Session handler assigned by session_set_save_handler().
 *
 * Cleans up a specific session.
 *
 * @param $sid
 *   Session ID.
 */
function _drupal_session_destroy($sid) {
  global $user, $is_https;

  // Nothing to do if we are not allowed to change the session.
  if (!drupal_save_session()) {
    return TRUE;
  }

  // Delete session data.
  //删除令牌，服务器端不保存会话数据，所以不用删除，客户端令牌，也无法删除，只能等其自动失效


  // Reset $_SESSION and $user to prevent a new session from being started
  // in drupal_session_commit().
  $_SESSION = array();
  $user = drupal_anonymous_user();

  // Unset the session cookies.
  _drupal_session_delete_cookie(session_name());
  if ($is_https) {
    _drupal_session_delete_cookie(substr(session_name(), 1), FALSE);
  }
  elseif (variable_get('https', FALSE)) {
    _drupal_session_delete_cookie('S' . session_name(), TRUE);
  }

  return TRUE;
}

/**
 * Deletes the session cookie.
 *
 * @param $name
 *   Name of session cookie to delete.
 * @param boolean $secure
 *   Force the secure value of the cookie.
 */
function _drupal_session_delete_cookie($name, $secure = NULL) {
  global $is_https;
  if (isset($_COOKIE[$name]) || (!$is_https && $secure === TRUE)) {
    $params = session_get_cookie_params();
    if ($secure !== NULL) {
      $params['secure'] = $secure;
    }
    $options = array(
      'expires' => REQUEST_TIME - 3600,
      'path' => $params['path'],
      'domain' => $params['domain'],
      'secure' => $params['secure'],
      'httponly' => $params['httponly'],
    );
    drupal_setcookie($name, '', $options);
    unset($_COOKIE[$name]);
  }
}






/**
 * Ends a specific user's session(s).
 *
 * @param $uid
 *   User ID.
 */
function drupal_session_destroy_uid($uid) {
  // Nothing to do if we are not allowed to change the session.
  if (!drupal_save_session()) {
    return;
  }
  //本功能无法实现，服务端没有状态存储，一旦用户获取会话，只能等其自然过期
}

/**
 * Session handler assigned by session_set_save_handler().
 *
 * Cleans up stalled sessions.
 *
 * @param $lifetime
 *   The value of session.gc_maxlifetime, passed by PHP.
 *   Sessions not updated for more than $lifetime seconds will be removed.
 */
function _drupal_session_garbage_collection($lifetime) {
  // Be sure to adjust 'php_value session.gc_maxlifetime' to a large enough
  // value. For example, if you want user sessions to stay in your database
  // for three weeks before deleting them, you need to set gc_maxlifetime
  // to '1814400'. At that value, only after a user doesn't log in after
  // three weeks (1814400 seconds) will his/her session be removed.
  //基于令牌的会话存储，服务器端不需要维护状态，只能通过cookie的过期时间进行控制
  return TRUE;
}



/**
 * Determines whether to save session data of the current request.
 *
 * This function allows the caller to temporarily disable writing of
 * session data, should the request end while performing potentially
 * dangerous operations, such as manipulating the global $user object.
 * See http://drupal.org/node/218104 for usage.
 *
 * @param $status
 *   Disables writing of session data when FALSE, (re-)enables
 *   writing when TRUE.
 *
 * @return
 *   FALSE if writing session data has been disabled. Otherwise, TRUE.
 */
function drupal_save_session($status = NULL) {
  // PHP session ID, session, and cookie handling happens in the global scope.
  // This value has to persist across calls to drupal_static_reset(), since a
  // potentially wrong or disallowed session would be written otherwise.
  static $save_session = TRUE;
  if (isset($status)) {
    $save_session = $status;
  }
  return $save_session;
}

/**
*获取用户身份信息
*/
function parse_token($tokens){
  require_once DRUPAL_ROOT . '/sites/all/modules/contrib/oauth2_server/oauth2_server.module';
 // Get parameters from the session, and then clean up.
  $parameters = array(
    'destination' => 'user',
    'op' => 'login',
    'connect_uid' => NULL,
  );
   $destination = $parameters['destination'];
    // Process the login 
	
    if ($tokens) {
      if ($parameters['op'] === 'login') {
		list($headerb64, $claims64, $signatureb64) = explode('.', $tokens['id_token']);
	$claims = json_decode(oauth2_server_base64url_decode($claims64), TRUE);
	if($claims['exp']< time()){
	   return '';
	}
	$signature = oauth2_server_base64url_decode($signatureb64);
	$payload = utf8_decode($headerb64 . '.' . $claims64);
	$keys = oauth2_server_get_keys();
  //oauth2/certificates
	 //return $claims;
    $verified = openssl_verify($payload, $signature, $keys['public_key'], 'sha256');
    return $verified? $claims:'';
		
      // return $user_data = $client->decodeIdToken($tokens['id_token']);
      
    }
  }
}

