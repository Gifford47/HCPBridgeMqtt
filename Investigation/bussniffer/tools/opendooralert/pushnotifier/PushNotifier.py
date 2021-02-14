import requests
import json
import base64
import uuid
from pushnotifier.exceptions import *


class PushNotifier:

    def __init__(self, username, password, package_name, api_key):
        """
        Initialize a new PushNotifier object

        Args:
            username (str): your username for https://pushnotifier.de
            password (str): your password for https://pushnotifier.de
            package_name (str): the package you want to send the messages to
            api_key (str): your api key (https://pushnotifier.de/account/api)
        """
        self.base_url = 'https://api.pushnotifier.de/v2'
        self.login_url = self.base_url + '/user/login'
        self.devices_url = self.base_url + '/devices'
        self.refresh_url = self.base_url + '/user/refresh'
        self.send_text_url = self.base_url + '/notifications/text'
        self.send_image_url = self.base_url + '/notifications/image'
        self.username = username
        self.package_name = package_name
        self.api_key = api_key
        self.app_token = self.__get_app_token(password)
        self.headers = {'X-AppToken': self.app_token}

    def login(self, password):
        """
        Used to verify everything is working fine

        Args:
            password (str): your password for https://pushnotifier.de

        Returns:
            dict: basic information about your account
        """

        login_data = {
            'username': self.username,
            'password': password
        }
        r = requests.post(self.login_url, json=login_data, auth=(
            self.package_name, self.api_key), headers=self.headers)
        return r.json()

    def __get_app_token(self, password):
        login_data = {
            'username': self.username,
            'password': password
        }
        
        r = requests.post(self.login_url, data=login_data, auth=(self.package_name, self.api_key))

        if r.status_code == 401:
            raise UnauthorizedError
        elif r.status_code == 403:
            raise IncorrectCredentialsError
        elif r.status_code == 404:
            raise UserNotFoundError        
        app_token = json.loads(r.text)['app_token']
        return app_token

    def refresh_token(self):
        """
        Used to refresh your app token

        Returns:
            str: new app token
        """
        r = requests.get(self.refresh_url, auth=(
            self.package_name, self.api_key), headers=self.headers)
        new_token = r.json()['app_token']
        self.app_token = new_token
        return new_token

    def get_all_devices(self):
        """
        Get all devices linked with your account

        Returns:
            list: list with all devices linked with your account

        """
        r = requests.get(self.devices_url, auth=(
            self.package_name, self.api_key), headers=self.headers)
        devices = r.json()
        devices_array = []
        for index, _ in enumerate(devices):
            devices_array.append(devices[index]['id'])
        return devices_array

    def send_text(self, text, devices=None, silent=False):
        """
        Sends a text to all devices specified

        Args:
            text (str): the text you want to send
            devices (list): a list of all devices you want to send the text to
            silent (bool): if False the message triggers a sound

        Returns:
            int: error code or 200 if everything went fine

        Raises:
            MalformedRequestError: the request is malformed, i.e. missing content
            DeviceNotFoundError: a device couldn\'t be found

        """

        if devices == None:
            body = {
                "devices": self.get_all_devices(),
                "content": text,
                "silent": silent
            }
        else:
            body = {
                "devices": devices,
                "content": text,
                "silent": silent
            }

        r = requests.put(self.send_text_url, json=body, auth=(
            self.package_name, self.api_key), headers=self.headers)
        if r.status_code == 200:
            return 200
        elif r.status_code == 400:
            raise MalformedRequestError
        elif r.status_code == 404:
            raise DeviceNotFoundError

    def send_url(self, url, devices=None, silent=False):
        """
        Sends a url to all devices specified

        Args:
            url (str): the url you want to send
            devices (list): a list of all devices you want to send the url to
            silent (bool): if False the message triggers a sound

        Returns:
            int: error code or 200 if everything went fine

        Raises:
            MalformedRequestError: the request is malformed, i.e. missing content
            DeviceNotFoundError: a device couldn\'t be found

        """
        if devices == None:
            body = {
                "devices": self.get_all_devices(),
                "content": url,
                "silent": silent
            }
        else:
            body = {
                "devices": devices,
                "content": url,
                "silent": silent
            }

        r = requests.put(self.send_text_url, json=body, auth=(
            self.package_name, self.api_key), headers=self.headers)

        if r.status_code == 200:
            return 200
        elif r.status_code == 400:
            raise MalformedRequestError
        elif r.status_code == 404:
            raise DeviceNotFoundError

    def send_notification(self, text, url, devices=None, silent=False):
        """
        Sends a notification (text + url) to all devices specified

        Args:
            text (str): the text you want to send
            url (str): the url you want to send
            devices (list): a list of all devices you want to send the notification to
            silent (bool): if False the message triggers a sound

        Returns:
            int: error code or 200 if everything went fine

        Raises:
            MalformedRequestError: the request is malformed, i.e. missing content
            DeviceNotFoundError: a device couldn\'t be found

        """
        if devices == None:
            body = {
                "devices": self.get_all_devices(),
                "content": text,
                "url": url,
                "silent": silent
            }
        else:
            body = {
                "devices": devices,
                "content": text,
                "url": url,
                "silent": silent
            }

        r = requests.put(self.send_text_url, json=body, auth=(
            self.package_name, self.api_key), headers=self.headers)

        if r.status_code == 200:
            return 200
        elif r.status_code == 400:
            raise MalformedRequestError
        elif r.status_code == 404:
            raise DeviceNotFoundError

    def send_image(self, image_path, devices=None, silent=False):
        """
        Thanks to @Logxn (github/logxn) for this method
        Sends an image to all devices specified

        Args:
            image_path (str): the path to the image you want to send
            devices (list): a list of all devices you want to send the image to
            silent (bool): if False the message triggers a sound

        Returns:
            int: error code or 200 if everything went fine

        Raises:
            MalformedRequestError: the request is malformed, i.e. missing content
            DeviceNotFoundError: a device couldn\'t be found
            PayloadTooLargeError: your image is too big (> 5 MB)
            UnsupportedMediaTypeError: you passed an invalid file type or the device(s) you tried to send this image to can\'t receive images
        """
        with open(image_path, 'rb') as image_file:
            encoded_bytes = base64.b64encode(image_file.read())

        # encoded_image are base64 encoded bytes of the image_file bytes
        # since json can't handle raw bytes we need to decode them into a base64 string
        encoded_image = encoded_bytes.decode()

        # generate random file name
        file_name = str(uuid.uuid4())

        if devices == None:
            body = {
                "devices": self.get_all_devices(),
                "content": encoded_image,
                "filename": file_name,
                "silent": silent
            }
        else:
            body = {
                "devices": devices,
                "content": encoded_image,
                "filename": file_name,
                "silent": silent
            }

        r = requests.put(self.send_image_url, json=body, auth=(
            self.package_name, self.api_key), headers=self.headers)

        if r.status_code == 200:
            return 200
        elif r.status_code == 400:
            raise MalformedRequestError
        elif r.status_code == 404:
            raise DeviceNotFoundError
        elif r.status_code == 413:
            raise PayloadTooLargeError
        elif r.status_code == 415:
            raise UnsupportedMediaTypeError
        else:
            raise UnknownError
