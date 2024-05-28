#pragma once
#include <iostream>
#include <string>
#include <queue>
#include <cassert>
#include <memory>

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include "Logger.h"

namespace Candela
{
	enum EventTypes
	{
		KeyPress = 0,
		KeyRelease,
		MousePress,
		MouseRelease,
		MouseScroll,
		MouseMove,
		WindowResize,
		Undefined
	};

	struct Event
	{
		EventTypes type; // The type of event that has occured 

		GLFWwindow* window; // The window in which the event occured
		int key; // The key that was pressed or released
		int button; // The mouse button that was pressed
		int mods; // The modifiers that were pressed with the key such as (CTRL, ALT. etc)
		int wx, wy; // Window Width and window height
		double mx, my; // Mouse X and Mouse Y
		double msx, msy; // Mouse scroll X and Mouse scroll y (The mouse scroll offset)
		double ts; // Event Timestep
	};

	class Application
	{
	public:
		Application();
		~Application();
		void Initialize();
		void OnUpdate();
		void FinishFrame();
		inline GLFWwindow* GetWindow() { return m_Window; }
		double GetTime();
		uint64_t GetCurrentFrame();
		unsigned int GetWidth();
		unsigned int GetHeight();
		void SetCursorLocked(bool locked);
		inline bool GetCursorLocked() noexcept { return m_CursorLocked; }

		bool* GetDebugCallbackOutputFlagPtr();

		inline float GetCursorX()
		{
			double x, y;

			glfwGetCursorPos(m_Window, &x, &y);
			return static_cast<float>(x);
		}

		inline float GetCursorY()
		{
			double x, y;

			glfwGetCursorPos(m_Window, &x, &y);
			return static_cast<float>(y);
		}

	protected:
		GLFWwindow* m_Window;
		unsigned int m_Width = 800;
		unsigned int m_Height = 600;
		std::string m_Appname;

		/*
		Pure virtual functions
		*/
		virtual void OnUserUpdate(double ts) = 0;
		virtual void OnUserCreate(double ts) = 0;
		virtual void OnImguiRender(double ts) = 0;
		virtual void OnEvent(Event e) = 0;

	private:
		void PollEvents();
		uint64_t m_CurrentFrame;
		std::queue<Event> m_EventQueue;
		int m_CurrentWidth = 0;
		int m_CurrentHeight = 0;
		bool m_CursorLocked = false;
	};
}