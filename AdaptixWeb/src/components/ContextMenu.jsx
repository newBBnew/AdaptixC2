import React, { useEffect, useRef } from 'react';
import { createPortal } from 'react-dom';
import { ChevronRight } from 'lucide-react';
import { cn } from '../utils/cn';

const ContextMenu = ({ x, y, options, onClose }) => {
  const menuRef = useRef(null);

  useEffect(() => {
    const handleClickOutside = (event) => {
      if (menuRef.current && !menuRef.current.contains(event.target)) {
        onClose();
      }
    };
    document.addEventListener('mousedown', handleClickOutside);
    // Handle scroll to close menu
    window.addEventListener('scroll', onClose, true);
    return () => {
      document.removeEventListener('mousedown', handleClickOutside);
      window.removeEventListener('scroll', onClose, true);
    };
  }, [onClose]);

  // Adjust position to keep within viewport
  const style = {
    top: y,
    left: x,
  };
  
  if (menuRef.current) {
    const rect = menuRef.current.getBoundingClientRect();
    if (x + rect.width > window.innerWidth) {
      style.left = x - rect.width;
    }
    if (y + rect.height > window.innerHeight) {
      style.top = y - rect.height;
    }
  }

  return createPortal(
    <div 
      ref={menuRef}
      className="fixed z-[9999] glass-panel border border-theme-glass-light rounded-xl shadow-2xl py-1.5 min-w-[200px] animate-in fade-in zoom-in duration-75"
      style={style}
    >
      {options.map((option, idx) => (
        option.divider ? (
          <div key={idx} className="h-px bg-theme-glass-light my-1.5 mx-2" />
        ) : (
          <div key={idx} className="relative group/sub">
            <button
              onClick={() => {
                if (!option.children) {
                  option.onClick();
                  onClose();
                }
              }}
              disabled={option.disabled}
              className={cn(
                "w-full text-left px-4 py-2 text-xs transition-all flex items-center justify-between group",
                option.disabled ? "opacity-30 cursor-not-allowed" : "hover:bg-theme-hover cursor-default",
                option.color || "text-theme-primary"
              )}
            >
              <div className="flex items-center space-x-3">
                {option.icon && <option.icon className={cn("w-4 h-4", !option.disabled && "text-theme-accent")} />}
                <span className="font-semibold tracking-tight uppercase text-[10px]">{option.label}</span>
              </div>
              {option.children && <ChevronRight size={14} className="text-theme-muted group-hover:text-theme-accent" />}
            </button>

            {/* Nested Submenu */}
            {option.children && (
              <div className="hidden group-hover/sub:block absolute left-full top-0 ml-[-4px] glass-panel border border-theme-glass-light rounded-xl shadow-2xl py-1.5 min-w-[200px]">
                {option.children.map((child, cIdx) => (
                  child.divider ? (
                    <div key={cIdx} className="h-px bg-theme-glass-light my-1.5 mx-2" />
                  ) : (
                    <button
                      key={cIdx}
                      onClick={() => {
                        child.onClick();
                        onClose();
                      }}
                      className={cn(
                        "w-full text-left px-4 py-2 text-xs transition-all flex items-center space-x-3 hover:bg-theme-hover cursor-default text-theme-primary"
                      )}
                    >
                      {child.icon && <child.icon className="w-4 h-4 text-theme-accent" />}
                      <span className="font-semibold tracking-tight uppercase text-[10px]">{child.label}</span>
                    </button>
                  )
                ))}
              </div>
            )}
          </div>
        )
      ))}
    </div>,
    document.body
  );
};

export default ContextMenu;
