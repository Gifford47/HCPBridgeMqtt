MalformedRequestError = Exception('the request is malformed, i.e. missing content')
DeviceNotFoundError = Exception('a device couldn\'t be found')
UserNotFoundError = Exception('user couldn\'t be found (incorrect username/password)')
IncorrectCredentialsError = Exception('credentials are incorrect')
UnauthorizedError = Exception('package name or api key is incorrect')
PayloadTooLargeError = Exception('your image is too big (> 5 MB)')
UnsupportedMediaTypeError = Exception('you passed an invalid file type or the device(s) you tried to send this image to can\'t receive images')

UnknownError = Exception('an unknown error occured! please contact the author of this module!')
